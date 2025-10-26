// browser_core.cpp
// Implementations for the small HTTP client and HTML extractor.

#include "browser_core.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socklen_t = int;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#define INVALID_SOCKET (-1)
#define SOCKET int
#endif

// --- Networking helpers --------------------------------------------------

static void init_sockets() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

static void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

static void close_socket(SOCKET s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

bool parse_url(const string &url, UrlParts &out) {
    string s = url;
    auto pos = s.find("://");
    if (pos == string::npos) return false;
    out.scheme = s.substr(0, pos);
    std::string rest = s.substr(pos + 3);
    auto p2 = rest.find('/');
    string hostport = (p2 == string::npos) ? rest : rest.substr(0, p2);
    out.path = (p2 == string::npos) ? "/" : rest.substr(p2);
    auto pcol = hostport.find(':');
    if (pcol == string::npos) {
        out.host = hostport;
        out.port = (out.scheme == "https") ? 443 : 80;
    } else {
        out.host = hostport.substr(0, pcol);
        out.port = stoi(hostport.substr(pcol + 1));
    }
    if (out.scheme != "http") return false;
    return true;
}

HttpResponse http_get(const string &url, int timeout_seconds) {
    UrlParts up;
    if (!parse_url(url, up)) {
        throw std::runtime_error("Only http:// URLs supported (simple parser)");
    }

    init_sockets();

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_s = std::to_string(up.port);
    struct addrinfo *res = nullptr;
    int rc = getaddrinfo(up.host.c_str(), port_s.c_str(), &hints, &res);
    if (rc != 0 || res == nullptr) {
        cleanup_sockets();
        throw std::runtime_error("getaddrinfo failed for host: " + up.host);
    }

    SOCKET s = INVALID_SOCKET;
    struct addrinfo *p;
    for (p = res; p != nullptr; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, p->ai_addr, (int)p->ai_addrlen) == 0) break;
        close_socket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (s == INVALID_SOCKET) {
        cleanup_sockets();
        throw std::runtime_error("Could not connect to " + up.host);
    }

    std::ostringstream req;
    req << "GET " << up.path << " HTTP/1.0\r\n";
    req << "Host: " << up.host << "\r\n";
    req << "User-Agent: MiniC++Browser/0.1\r\n";
    req << "Connection: close\r\n";
    req << "\r\n";

    string reqs = req.str();
    size_t sent = 0;
    while (sent < reqs.size()) {
        int n = (int)send(s, reqs.data() + sent, (int)(reqs.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }

    std::vector<char> buf;
    const int CHUNK = 4096;
    char tmp[CHUNK];
    for (;;) {
        int n = (int)recv(s, tmp, CHUNK, 0);
        if (n <= 0) break;
        buf.insert(buf.end(), tmp, tmp + n);
    }

    close_socket(s);
    cleanup_sockets();

    string raw(buf.begin(), buf.end());
    string sep = "\r\n\r\n";
    auto pos = raw.find(sep);
    string head, body;
    if (pos != string::npos) {
        head = raw.substr(0, pos);
        body = raw.substr(pos + sep.size());
    } else {
        head = raw;
        body = "";
    }

    std::istringstream sh(head);
    string line;
    HttpResponse resp;
    if (std::getline(sh, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        resp.status_line = line;
    }

    while (std::getline(sh, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto pcol = line.find(':');
        if (pcol != string::npos) {
            string k = line.substr(0, pcol);
            string v = line.substr(pcol + 1);
            while (!v.empty() && std::isspace((unsigned char)v.front())) v.erase(v.begin());
            std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c){ return std::tolower(c); });
            resp.headers[k] = v;
        }
    }
    resp.body = body;
    return resp;
}

void extract_text_and_links(const string &html, string &out_text, std::vector<std::pair<string,string>> &out_links) {
    out_text.clear();
    out_links.clear();
    size_t i = 0;
    while (i < html.size()) {
        if (html[i] == '<') {
            size_t j = html.find('>', i+1);
            if (j == string::npos) break;
            string tag = html.substr(i+1, j - i - 1);
            string ltag = tag;
            std::transform(ltag.begin(), ltag.end(), ltag.begin(), [](unsigned char c){ return std::tolower(c); });
            if (ltag.rfind("a ", 0) == 0 || ltag == "a" || ltag.find("a ") != string::npos) {
                auto href_pos = ltag.find("href");
                string href_val;
                if (href_pos != string::npos) {
                    auto eq = tag.find('=', href_pos);
                    if (eq != string::npos) {
                        size_t q = tag.find_first_of("\'\"", eq+1);
                        if (q != string::npos) {
                            char qq = tag[q];
                            size_t q2 = tag.find(qq, q+1);
                            if (q2 != string::npos) {
                                href_val = tag.substr(q+1, q2 - q - 1);
                            }
                        } else {
                            size_t sp = tag.find(' ', eq+1);
                            href_val = tag.substr(eq+1, (sp==string::npos? tag.size(): sp) - (eq+1));
                        }
                    }
                }
                size_t close = html.find("</a>", j+1);
                size_t text_start = j+1;
                size_t text_end = (close == string::npos) ? html.size() : close;
                string inner = html.substr(text_start, text_end - text_start);
                string inner_text;
                for (size_t k = 0; k < inner.size();) {
                    if (inner[k] == '<') {
                        size_t k2 = inner.find('>', k+1);
                        if (k2 == string::npos) break;
                        k = k2 + 1;
                    } else {
                        inner_text.push_back(inner[k]);
                        ++k;
                    }
                }
                out_text += inner_text;
                out_links.emplace_back(inner_text, href_val);
                i = (close == string::npos) ? html.size() : (close + 4);
                continue;
            } else {
                i = j + 1;
                continue;
            }
        } else {
            out_text.push_back(html[i]);
            ++i;
        }
    }
}

namespace browser {
    RenderContext parse_document(const string &html, const string &css) {
        RenderContext ctx;
        ctx.document = parse_html(html);
        if (!css.empty()) {
            ctx.stylesheet = parse_css(css);
        }
        return ctx;
    }
}
