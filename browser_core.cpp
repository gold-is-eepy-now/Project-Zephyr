#include "browser_core.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <cstring>
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

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string collapse_whitespace(const std::string& s) {
    std::string out;
    bool in_ws = false;
    for (unsigned char ch : s) {
        if (std::isspace(ch)) {
            if (!in_ws) out.push_back(' ');
            in_ws = true;
        } else {
            out.push_back(static_cast<char>(ch));
            in_ws = false;
        }
    }
    return trim(out);
}

std::string decode_html_entities(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            const size_t semicolon = text.find(';', i + 1);
            if (semicolon != std::string::npos && semicolon - i <= 10) {
                const std::string entity = text.substr(i + 1, semicolon - i - 1);
                if (entity == "amp") out.push_back('&');
                else if (entity == "lt") out.push_back('<');
                else if (entity == "gt") out.push_back('>');
                else if (entity == "quot") out.push_back('"');
                else if (entity == "#39") out.push_back('\'');
                else if (entity == "nbsp") out.push_back(' ');
                else if (!entity.empty() && entity[0] == '#') {
                    try {
                        int code = 0;
                        if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X')) code = std::stoi(entity.substr(2), nullptr, 16);
                        else code = std::stoi(entity.substr(1), nullptr, 10);
                        if (code > 0 && code <= 255) out.push_back(static_cast<char>(code));
                    } catch (...) { out += '&' + entity + ';'; }
                } else {
                    out += '&' + entity + ';';
                }
                i = semicolon;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

std::string normalize_path(const std::string& path) {
    std::vector<std::string> segs;
    std::stringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") continue;
        if (segment == "..") {
            if (!segs.empty()) segs.pop_back();
            continue;
        }
        segs.push_back(segment);
    }

    std::string out = "/";
    for (size_t i = 0; i < segs.size(); ++i) {
        out += segs[i];
        if (i + 1 < segs.size()) out += '/';
    }
    return out;
}

std::string extract_tag_attribute(const std::string& tag_text, const std::string& attr_name) {
    const std::string lower = to_lower(tag_text);
    const std::string needle = to_lower(attr_name);
    const auto pos = lower.find(needle);
    if (pos == std::string::npos) return "";

    const auto eq = tag_text.find('=', pos);
    if (eq == std::string::npos) return "";

    size_t start = eq + 1;
    while (start < tag_text.size() && std::isspace(static_cast<unsigned char>(tag_text[start]))) ++start;
    if (start >= tag_text.size()) return "";

    if (tag_text[start] == '"' || tag_text[start] == '\'') {
        const char quote = tag_text[start++];
        const size_t end = tag_text.find(quote, start);
        if (end == std::string::npos) return "";
        return trim(tag_text.substr(start, end - start));
    }

    const size_t end = tag_text.find_first_of(" \t\r\n", start);
    return trim(tag_text.substr(start, end - start));
}

#ifdef ZEPHYR_USE_CURL
size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    const size_t bytes = size * nmemb;
    if (body->size() + bytes > kMaxResponseBytes) return 0;
    body->append(ptr, bytes);
    return bytes;
}

size_t curl_header_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response = static_cast<HttpResponse*>(userdata);
    const size_t bytes = size * nmemb;
    std::string line(ptr, bytes);
    line = trim(line);
    if (line.empty()) return bytes;

    if (line.rfind("HTTP/", 0) == 0) {
        response->status_line = line;
    } else {
        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            response->headers[to_lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
        }
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

void set_socket_timeouts(SOCKET s, int timeout_seconds) {
#ifdef _WIN32
    DWORD timeout_ms = static_cast<DWORD>(timeout_seconds * 1000);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    timeval tv {};
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}
#endif

}  // namespace

bool parse_url(const string& url, UrlParts& out) {
    const auto scheme_pos = url.find("://");
    if (scheme_pos == string::npos) return false;
    out.scheme = to_lower(url.substr(0, scheme_pos));

    const std::string rest = url.substr(scheme_pos + 3);
    const auto path_pos = rest.find('/');
    std::string host_port = (path_pos == std::string::npos) ? rest : rest.substr(0, path_pos);
    out.path = (path_pos == std::string::npos) ? "/" : rest.substr(path_pos);
    if (out.path.empty()) out.path = "/";

    const auto colon_pos = host_port.rfind(':');
    if (colon_pos != std::string::npos) {
        out.host = host_port.substr(0, colon_pos);
        out.port = std::stoi(host_port.substr(colon_pos + 1));
    } else {
        out.host = host_port;
        out.port = (out.scheme == "https") ? 443 : 80;
    }

    return out.scheme == "http" || out.scheme == "https";
}

bool is_safe_navigation_target(const string& href) {
    const std::string lower = to_lower(trim(href));
    return !(lower.rfind("javascript:", 0) == 0 ||
             lower.rfind("data:", 0) == 0 ||
             lower.rfind("file:", 0) == 0 ||
             lower.rfind("vbscript:", 0) == 0);
}

string resolve_url(const string& base_url, const string& href) {
    const std::string clean_href = trim(href);
    if (clean_href.empty() || !is_safe_navigation_target(clean_href)) return "";
    if (clean_href.rfind("http://", 0) == 0 || clean_href.rfind("https://", 0) == 0) return clean_href;

    UrlParts base;
    if (!parse_url(base_url, base)) return "";

    if (clean_href[0] == '#') return base_url;
    if (clean_href.rfind("//", 0) == 0) return base.scheme + ":" + clean_href;
    if (clean_href[0] == '/') return base.scheme + "://" + base.host + normalize_path(clean_href);

    std::string base_path = base.path;
    const auto slash = base_path.rfind('/');
    base_path = (slash == std::string::npos) ? "/" : base_path.substr(0, slash + 1);
    return base.scheme + "://" + base.host + normalize_path(base_path + clean_href);
}

HttpResponse http_get(const string& url, int timeout_seconds, int redirect_limit) {
    if (redirect_limit < 0) throw std::runtime_error("Redirect limit exceeded");

    UrlParts up;
    if (!parse_url(url, up)) throw std::runtime_error("Only http:// and https:// URLs supported");

#ifdef ZEPHYR_USE_CURL
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(redirect_limit));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ZephyrBrowser/3.0");
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

    const CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        const std::string err = curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        throw std::runtime_error("curl request failed: " + err);
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    response.status_line = "HTTP/1.1 " + std::to_string(status);

    curl_easy_cleanup(curl);
    return response;
#else
    init_sockets();
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const std::string port_s = std::to_string(up.port);
    if (getaddrinfo(up.host.c_str(), port_s.c_str(), &hints, &res) != 0 || res == nullptr) {
        cleanup_sockets();
        throw std::runtime_error("getaddrinfo failed for host: " + up.host);
    }

    SOCKET s = INVALID_SOCKET;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        set_socket_timeouts(s, timeout_seconds);
        if (connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) break;
        close_socket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (s == INVALID_SOCKET) {
        cleanup_sockets();
        throw std::runtime_error("Could not connect to " + up.host);
    }

    std::ostringstream req;
    req << "GET " << up.path << " HTTP/1.1\r\n";
    req << "Host: " << up.host << "\r\n";
    req << "User-Agent: ZephyrBrowser/3.0\r\n";
    req << "Connection: close\r\n\r\n";

    const string request = req.str();
    size_t sent = 0;
    while (sent < request.size()) {
        const int n = static_cast<int>(send(s, request.data() + sent, static_cast<int>(request.size() - sent), 0));
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }

    std::vector<char> buffer;
    constexpr int chunk = 4096;
    char tmp[chunk];
    for (;;) {
        const int n = static_cast<int>(recv(s, tmp, chunk, 0));
        if (n <= 0) break;
        buffer.insert(buffer.end(), tmp, tmp + n);
        if (buffer.size() > kMaxResponseBytes) {
            close_socket(s);
            cleanup_sockets();
            throw std::runtime_error("Response exceeded safe size limit");
        }
    }

    close_socket(s);
    cleanup_sockets();

    HttpResponse resp;
    resp.status_line = "HTTP/1.1 000";
    resp.body.assign(buffer.begin(), buffer.end());
    return resp;
#endif
}

void extract_text_and_links(const string& html, string& out_text, std::vector<std::pair<string, string>>& out_links) {
    out_text.clear();
    out_links.clear();

    const std::string lower_html = to_lower(html);
    bool in_script = false;
    bool in_style = false;

    for (size_t i = 0; i < html.size();) {
        if (!in_script && !in_style && html[i] != '<') {
            out_text.push_back(html[i++]);
            continue;
        }

        if (html[i] != '<') {
            ++i;
            continue;
        }

        const size_t tag_end = html.find('>', i + 1);
        if (tag_end == string::npos) break;

        std::string tag = html.substr(i + 1, tag_end - i - 1);
        std::string tag_lower = to_lower(trim(tag));

        if (tag_lower.rfind("script", 0) == 0) { in_script = true; i = tag_end + 1; continue; }
        if (tag_lower == "/script") { in_script = false; i = tag_end + 1; continue; }
        if (tag_lower.rfind("style", 0) == 0) { in_style = true; i = tag_end + 1; continue; }
        if (tag_lower == "/style") { in_style = false; i = tag_end + 1; continue; }
        if (in_script || in_style) { i = tag_end + 1; continue; }

        if (tag_lower.rfind("a", 0) == 0) {
            std::string href = extract_tag_attribute(tag, "href");
            const size_t close = lower_html.find("</a>", tag_end + 1);
            const size_t content_end = (close == string::npos) ? html.size() : close;
            const std::string inner = html.substr(tag_end + 1, content_end - tag_end - 1);

            std::string text;
            bool in_tag = false;
            for (char c : inner) {
                if (c == '<') in_tag = true;
                else if (c == '>') in_tag = false;
                else if (!in_tag) text.push_back(c);
            }

            const std::string clean_text = collapse_whitespace(decode_html_entities(text));
            if (!clean_text.empty() && is_safe_navigation_target(href)) {
                out_links.emplace_back(clean_text, trim(href));
                out_text += clean_text + "\n";
            }

            i = (close == string::npos) ? html.size() : close + 4;
            continue;
        }

        i = tag_end + 1;
    }

    out_text = collapse_whitespace(decode_html_entities(out_text));
}

string extract_style_blocks(const string& html) {
    std::string css;
    const std::string lower = to_lower(html);
    size_t pos = 0;

    while ((pos = lower.find("<style", pos)) != string::npos) {
        const size_t open_end = lower.find('>', pos);
        if (open_end == string::npos) break;
        const size_t close = lower.find("</style>", open_end + 1);
        if (close == string::npos) break;

        css += html.substr(open_end + 1, close - open_end - 1);
        css += "\n";
        pos = close + 8;
    }

    return css;
}

SourceBundle extract_source_bundle(const string& html) {
    SourceBundle b;
    b.html = html;
    b.css = extract_style_blocks(html);

    const std::string lower = to_lower(html);
    size_t pos = 0;

    while ((pos = lower.find("<script", pos)) != string::npos) {
        const size_t open_end = lower.find('>', pos);
        if (open_end == string::npos) break;

        const std::string open_tag = html.substr(pos + 1, open_end - pos - 1);
        const std::string type_attr = to_lower(extract_tag_attribute(open_tag, "type"));
        const std::string src_attr = extract_tag_attribute(open_tag, "src");
        const bool is_typescript = type_attr.find("typescript") != std::string::npos || type_attr.find("text/ts") != std::string::npos;

        const size_t close = lower.find("</script>", open_end + 1);
        std::string body;
        if (close != string::npos) body = html.substr(open_end + 1, close - open_end - 1);

        std::string block;
        if (!src_attr.empty()) block += "// external script src=" + src_attr + "\n";
        if (!trim(body).empty()) {
            block += body;
            if (block.back() != '\n') block.push_back('\n');
        }

        if (!trim(block).empty()) {
            if (is_typescript) b.typescript += block + "\n";
            else b.javascript += block + "\n";
        }

        pos = (close == string::npos) ? html.size() : close + 9;
    }

    return b;
}


string render_page_text(const string& html, size_t wrap_width) {
    const std::string css = extract_style_blocks(html);
    browser::RenderContext ctx = browser::parse_document(html, css);
    if (!ctx.document) return "";

    auto is_block_tag = [](const std::string& tag) {
        static const std::unordered_set<std::string> blocks = {
            "html", "body", "main", "article", "section", "header", "footer", "nav", "aside",
            "div", "p", "pre", "blockquote", "ul", "ol", "li", "table", "tr", "td", "th",
            "h1", "h2", "h3", "h4", "h5", "h6", "form", "fieldset", "legend"};
        return blocks.count(tag) != 0;
    };

    auto should_skip_tag = [](const std::string& tag) {
        return tag == "script" || tag == "style" || tag == "noscript" || tag == "meta" || tag == "link";
    };

    auto display_none = [&](const browser::ElementPtr& el) {
        auto style = ctx.stylesheet.computeStyle(el);
        if (style.has_display && style.display == "none") return true;
        const std::string inline_style = to_lower(el->getAttribute("style"));
        return inline_style.find("display:none") != std::string::npos;
    };

    std::string out;
    size_t line_len = 0;

    auto newline = [&]() {
        out.push_back('\n');
        line_len = 0;
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
            std::string word;
            while (iss >> word) {
                if (line_len > 0) {
                    if (line_len + 1 + word.size() > wrap_width) newline();
                    else {
                        out.push_back(' ');
                        ++line_len;
                    }
                }
                out += word;
                line_len += word.size();
            }
            return;
        }

        auto el = std::dynamic_pointer_cast<browser::Element>(node);
        if (!el) return;
        const std::string tag = el->tag_name;
        if (should_skip_tag(tag) || display_none(el)) return;

        const bool is_block = is_block_tag(tag);
        if (tag == "br") newline();
        if (is_block && line_len > 0) newline();

        if (tag == "li") {
            if (line_len > 0) newline();
            out += "- ";
            line_len = 2;
        }

        for (const auto& child : el->children) walk(child);

        if (tag == "a") {
            std::string href = el->getAttribute("href");
            if (!href.empty() && is_safe_navigation_target(href)) {
                std::string suffix = " (" + href + ")";
                if (line_len + suffix.size() > wrap_width && line_len > 0) newline();
                out += suffix;
                line_len += suffix.size();
            }
        }

        if (is_block && line_len > 0) newline();
    };

    walk(ctx.document);

    while (out.find("\n\n\n") != std::string::npos) {
        out.replace(out.find("\n\n\n"), 3, "\n\n");
    }

    return trim(out);
}

namespace browser {
RenderContext parse_document(const string& html, const string& css) {
    RenderContext ctx;
    ctx.document = parse_html(html);
    if (!css.empty()) ctx.stylesheet = parse_css(css);
    return ctx;
}
}  // namespace browser
