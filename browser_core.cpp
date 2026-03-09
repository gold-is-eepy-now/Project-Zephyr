#include "browser_core.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#ifdef ZEPHYR_USE_CURL
#include <curl/curl.h>
#endif

#ifndef ZEPHYR_USE_CURL
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socklen_t = int;
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define INVALID_SOCKET (-1)
#define SOCKET int
#endif
#endif

namespace {
constexpr size_t kMaxResponseBytes = 2 * 1024 * 1024;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string collapse_whitespace(const std::string& s) {
    std::string out;
    bool ws = false;
    for (unsigned char ch : s) {
        if (std::isspace(ch)) {
            ws = true;
        } else {
            if (ws && !out.empty()) out.push_back(' ');
            ws = false;
            out.push_back(static_cast<char>(ch));
        }
    }
    return trim(out);
}

std::string decode_html_entities(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            out.push_back(text[i]);
            continue;
        }

        const size_t semi = text.find(';', i + 1);
        if (semi == std::string::npos) {
            out.push_back('&');
            continue;
        }

        const std::string ent = text.substr(i + 1, semi - i - 1);
        if (ent == "amp") out.push_back('&');
        else if (ent == "lt") out.push_back('<');
        else if (ent == "gt") out.push_back('>');
        else if (ent == "quot") out.push_back('"');
        else if (ent == "apos" || ent == "#39") out.push_back('\'');
        else if (ent == "nbsp") out.push_back(' ');
        else if (!ent.empty() && ent[0] == '#') {
            int code = -1;
            try {
                if (ent.size() > 2 && (ent[1] == 'x' || ent[1] == 'X')) code = std::stoi(ent.substr(2), nullptr, 16);
                else code = std::stoi(ent.substr(1));
            } catch (...) {
                code = -1;
            }
            if (code >= 0 && code <= 127) out.push_back(static_cast<char>(code));
        }

        i = semi;
    }
    return out;
}

std::string normalize_path(const std::string& path) {
    std::vector<std::string> segs;
    std::stringstream ss(path);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        if (seg.empty() || seg == ".") continue;
        if (seg == "..") {
            if (!segs.empty()) segs.pop_back();
            continue;
        }
        segs.push_back(seg);
    }

    std::string out = "/";
    for (size_t i = 0; i < segs.size(); ++i) {
        out += segs[i];
        if (i + 1 < segs.size()) out.push_back('/');
    }
    return out;
}

std::string extract_tag_attribute(const std::string& tag_text, const std::string& key) {
    const std::string low = lower(tag_text);
    const std::string needle = lower(key);

    size_t p = low.find(needle);
    while (p != std::string::npos) {
        const bool boundary_before = (p == 0) || std::isspace(static_cast<unsigned char>(low[p - 1])) || low[p - 1] == '<';
        const size_t after = p + needle.size();
        const bool boundary_after = (after >= low.size()) || std::isspace(static_cast<unsigned char>(low[after])) || low[after] == '=';
        if (boundary_before && boundary_after) break;
        p = low.find(needle, p + 1);
    }
    if (p == std::string::npos) return "";

    const size_t eq = tag_text.find('=', p + key.size());
    if (eq == std::string::npos) return "";

    size_t i = eq + 1;
    while (i < tag_text.size() && std::isspace(static_cast<unsigned char>(tag_text[i]))) ++i;
    if (i >= tag_text.size()) return "";

    if (tag_text[i] == '"' || tag_text[i] == '\'') {
        const char q = tag_text[i++];
        const size_t end = tag_text.find(q, i);
        return (end == std::string::npos) ? "" : trim(tag_text.substr(i, end - i));
    }

    size_t end = i;
    while (end < tag_text.size() && !std::isspace(static_cast<unsigned char>(tag_text[end])) && tag_text[end] != '>') ++end;
    return trim(tag_text.substr(i, end - i));
}

#ifdef ZEPHYR_USE_CURL
size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    auto* body = static_cast<std::string*>(userdata);
    if (body->size() + bytes > kMaxResponseBytes) return 0;
    body->append(ptr, bytes);
    return bytes;
}

size_t curl_header_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    auto* resp = static_cast<HttpResponse*>(userdata);
    std::string line = trim(std::string(ptr, bytes));
    if (line.empty()) return bytes;

    if (line.rfind("HTTP/", 0) == 0) {
        resp->status_line = line;
    } else {
        const size_t colon = line.find(':');
        if (colon != std::string::npos) resp->headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }
    return bytes;
}
#endif

#ifndef ZEPHYR_USE_CURL
void init_sockets() {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
}

void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void close_socket(SOCKET s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
#endif

}  // namespace

bool parse_url(const string& url, UrlParts& out) {
    const size_t scheme = url.find("://");
    if (scheme == string::npos) return false;

    out.scheme = lower(url.substr(0, scheme));
    if (out.scheme != "http" && out.scheme != "https") return false;

    const std::string rest = url.substr(scheme + 3);
    const size_t slash = rest.find('/');
    std::string host_port = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    out.path = (slash == std::string::npos) ? "/" : rest.substr(slash);
    if (out.path.empty()) out.path = "/";

    const size_t colon = host_port.rfind(':');
    if (colon != std::string::npos && host_port.find(']') == std::string::npos) {
        out.host = host_port.substr(0, colon);
        out.port = std::stoi(host_port.substr(colon + 1));
    } else {
        out.host = host_port;
        out.port = (out.scheme == "https") ? 443 : 80;
    }

    return !out.host.empty();
}

bool is_safe_navigation_target(const string& href) {
    const std::string h = lower(trim(href));
    return !(h.rfind("javascript:", 0) == 0 || h.rfind("data:", 0) == 0 || h.rfind("file:", 0) == 0 || h.rfind("vbscript:", 0) == 0);
}

string resolve_url(const string& base_url, const string& href) {
    const std::string clean = trim(href);
    if (clean.empty() || !is_safe_navigation_target(clean)) return "";
    if (clean.rfind("http://", 0) == 0 || clean.rfind("https://", 0) == 0) return clean;

    UrlParts base;
    if (!parse_url(base_url, base)) return "";

    if (clean[0] == '#') return base_url;
    if (clean.rfind("//", 0) == 0) return base.scheme + ":" + clean;
    if (clean[0] == '/') return base.scheme + "://" + base.host + normalize_path(clean);

    std::string dir = base.path;
    const size_t slash = dir.rfind('/');
    dir = (slash == std::string::npos) ? "/" : dir.substr(0, slash + 1);
    return base.scheme + "://" + base.host + normalize_path(dir + clean);
}

HttpResponse http_get(const string& url, int timeout_seconds, int redirect_limit) {
    UrlParts p;
    if (!parse_url(url, p)) throw std::runtime_error("Only http:// and https:// URLs are supported");

#ifdef ZEPHYR_USE_CURL
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl initialization failed");

    HttpResponse resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(redirect_limit));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Zephyr/Rewrite");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        const std::string err = curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        throw std::runtime_error("curl request failed: " + err);
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (resp.status_line.empty()) resp.status_line = "HTTP/1.1 " + std::to_string(status);
    curl_easy_cleanup(curl);
    return resp;
#else
    if (p.scheme == "https") throw std::runtime_error("HTTPS requires libcurl in this build");

    init_sockets();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const std::string port = std::to_string(p.port);
    if (getaddrinfo(p.host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
        cleanup_sockets();
        throw std::runtime_error("getaddrinfo failed");
    }

    SOCKET s = INVALID_SOCKET;
    for (auto* cur = res; cur; cur = cur->ai_next) {
        s = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, cur->ai_addr, static_cast<int>(cur->ai_addrlen)) == 0) break;
        close_socket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (s == INVALID_SOCKET) {
        cleanup_sockets();
        throw std::runtime_error("connection failed");
    }

    std::ostringstream req;
    req << "GET " << p.path << " HTTP/1.1\r\n";
    req << "Host: " << p.host << "\r\n";
    req << "Connection: close\r\n\r\n";

    std::string request = req.str();
    send(s, request.c_str(), static_cast<int>(request.size()), 0);

    std::string raw;
    char buf[4096];
    for (;;) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, n);
        if (raw.size() > kMaxResponseBytes) break;
    }

    close_socket(s);
    cleanup_sockets();

    HttpResponse resp;
    resp.status_line = "HTTP/1.1 000";
    resp.body = raw;
    return resp;
#endif
}

void extract_text_and_links(const string& html, string& out_text, std::vector<std::pair<string, string>>& out_links) {
    out_text.clear();
    out_links.clear();

    size_t i = 0;
    while (i < html.size()) {
        if (html[i] != '<') {
            out_text.push_back(html[i++]);
            continue;
        }

        size_t end = html.find('>', i + 1);
        if (end == string::npos) break;

        std::string tag = html.substr(i + 1, end - i - 1);
        std::string low = lower(trim(tag));

        if (low.rfind("a", 0) == 0) {
            std::string href = extract_tag_attribute(tag, "href");
            size_t close = lower(html).find("</a>", end + 1);
            size_t text_end = (close == string::npos) ? html.size() : close;
            std::string text = html.substr(end + 1, text_end - end - 1);
            text = collapse_whitespace(decode_html_entities(text));
            if (!text.empty() && is_safe_navigation_target(href)) out_links.emplace_back(text, trim(href));
        }

        i = end + 1;
    }

    out_text = collapse_whitespace(decode_html_entities(out_text));
}

string extract_style_blocks(const string& html) {
    std::string out;
    std::string low = lower(html);
    size_t i = 0;
    while ((i = low.find("<style", i)) != string::npos) {
        size_t open = low.find('>', i);
        if (open == string::npos) break;
        size_t close = low.find("</style>", open + 1);
        if (close == string::npos) break;
        out += html.substr(open + 1, close - open - 1) + "\n";
        i = close + 8;
    }
    return out;
}

SourceBundle extract_source_bundle(const string& html) {
    SourceBundle b;
    b.html = html;
    b.css = extract_style_blocks(html);

    const std::string low = lower(html);
    size_t i = 0;
    while ((i = low.find("<script", i)) != string::npos) {
        size_t open = low.find('>', i);
        if (open == string::npos) break;
        size_t close = low.find("</script>", open + 1);
        if (close == string::npos) break;

        std::string open_tag = html.substr(i + 1, open - i - 1);
        std::string type = lower(extract_tag_attribute(open_tag, "type"));
        std::string src = extract_tag_attribute(open_tag, "src");
        std::string body = html.substr(open + 1, close - open - 1);

        std::string block;
        if (!src.empty()) block += "// external script src=" + src + "\n";
        if (!trim(body).empty()) block += body + "\n";

        if (!trim(block).empty()) {
            if (type.find("typescript") != std::string::npos || type.find("text/ts") != std::string::npos) b.typescript += block + "\n";
            else b.javascript += block + "\n";
        }

        i = close + 9;
    }

    return b;
}

string render_page_text(const string& html, size_t wrap_width) {
    const std::string css = extract_style_blocks(html);
    browser::RenderContext ctx = browser::parse_document(html, css);
    if (!ctx.document) return "";

    auto is_block_tag = [](const std::string& tag) {
        static const std::unordered_set<std::string> blocks = {
            "html", "body", "main", "article", "section", "header", "footer", "nav", "aside", "div", "p", "ul", "ol", "li",
            "h1", "h2", "h3", "h4", "h5", "h6", "pre", "blockquote", "table", "tr", "td", "th", "form"};
        return blocks.count(tag) != 0;
    };

    auto should_skip_tag = [](const std::string& tag) {
        return tag == "script" || tag == "style" || tag == "noscript" || tag == "meta" || tag == "link" || tag == "head";
    };

    auto is_hidden = [&](const browser::ElementPtr& el) {
        const auto st = ctx.stylesheet.computeStyle(el);
        if (st.has_display && st.display == "none") return true;
        const std::string inline_style = lower(el->getAttribute("style"));
        return inline_style.find("display:none") != std::string::npos;
    };

    std::string out;
    size_t line = 0;
    auto newline = [&]() {
        if (!out.empty() && out.back() == '\n') return;
        out.push_back('\n');
        line = 0;
    };

    std::function<void(const browser::NodePtr&)> walk;
    walk = [&](const browser::NodePtr& node) {
        if (!node) return;
        if (node->type == browser::NodeType::TEXT) {
            auto t = std::dynamic_pointer_cast<browser::TextNode>(node);
            if (!t) return;
            std::string text = collapse_whitespace(decode_html_entities(t->text));
            if (text.empty()) return;

            std::istringstream iss(text);
            std::string w;
            while (iss >> w) {
                if (line > 0) {
                    if (line + 1 + w.size() > wrap_width) newline();
                    else {
                        out.push_back(' ');
                        ++line;
                    }
                }
                out += w;
                line += w.size();
            }
            return;
        }

        auto el = std::dynamic_pointer_cast<browser::Element>(node);
        if (!el) return;

        if (should_skip_tag(el->tag_name) || is_hidden(el)) return;

        const bool is_block = is_block_tag(el->tag_name);
        if (el->tag_name == "br") newline();
        if (is_block && line > 0) newline();
        if (el->tag_name == "li") {
            if (line > 0) newline();
            out += "- ";
            line = 2;
        }

        for (const auto& c : el->children) walk(c);

        if (el->tag_name == "a") {
            const std::string href = el->getAttribute("href");
            if (!href.empty() && is_safe_navigation_target(href)) {
                const std::string suffix = " (" + href + ")";
                if (line + suffix.size() > wrap_width && line > 0) newline();
                out += suffix;
                line += suffix.size();
            }
        }

        if (is_block && line > 0) newline();
    };

    walk(ctx.document);

    while (out.find("\n\n\n") != std::string::npos) out.replace(out.find("\n\n\n"), 3, "\n\n");
    return trim(out);
}

namespace browser {

RenderContext parse_document(const string& html, const string& css) {
    RenderContext r;
    r.document = parse_html(html);
    r.stylesheet = parse_css(css);
    return r;
}

}  // namespace browser
