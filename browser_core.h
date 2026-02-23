// browser_core.h
// Shared core: HTTP(S) client and lightweight HTML/CSS/JS source extraction helpers.
#pragma once

#include <map>
#include <string>
#include <vector>

using std::string;

struct HttpResponse {
    string status_line;
    std::map<string, string> headers;
    string body;
};

struct UrlParts {
    string scheme;
    string host;
    int port = 80;
    string path;
};

struct SourceBundle {
    string html;
    string css;
    string javascript;
    string typescript;
};

bool parse_url(const string& url, UrlParts& out);
bool is_safe_navigation_target(const string& href);
string resolve_url(const string& base_url, const string& href);
HttpResponse http_get(const string& url, int timeout_seconds = 10, int redirect_limit = 3);
void extract_text_and_links(const string& html, string& out_text, std::vector<std::pair<string, string>>& out_links);
string extract_style_blocks(const string& html);
SourceBundle extract_source_bundle(const string& html);
string render_page_text(const string& html, size_t wrap_width = 100);

#include "dom.h"
#include "css.h"

namespace browser {
struct RenderContext {
    ElementPtr document;
    StyleSheet stylesheet;
};

RenderContext parse_document(const string& html, const string& css = "");
}  // namespace browser
