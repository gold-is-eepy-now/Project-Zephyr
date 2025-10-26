// browser_core.h
// Shared core: HTTP client and tiny HTML extractor
#pragma once

#include <string>
#include <vector>
#include <map>

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

bool parse_url(const string &url, UrlParts &out);
HttpResponse http_get(const string &url, int timeout_seconds = 10);
void extract_text_and_links(const string &html, string &out_text, std::vector<std::pair<string,string>> &out_links);

// New DOM-based HTML parsing
#include "dom.h"
#include "css.h"

namespace browser {
    struct RenderContext {
        ElementPtr document;
        StyleSheet stylesheet;
    };
    
    RenderContext parse_document(const string &html, const string &css = "");
}
