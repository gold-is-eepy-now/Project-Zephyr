#include "browser_core.h"

#include <cassert>
#include <iostream>

int main() {
    UrlParts parts;
    assert(parse_url("http://example.com/a/b", parts));
    assert(parts.scheme == "http");
    assert(parts.host == "example.com");

    assert(parse_url("https://example.com", parts));
    assert(parts.port == 443);

    assert(resolve_url("https://example.com/a/b", "../c") == "https://example.com/c");
    assert(resolve_url("https://example.com/a/b", "javascript:alert(1)").empty());

    const std::string html =
        "<html><head><style>p{padding:4px;} span{display:none;}</style></head>"
        "<body><h1>Hello</h1><p id='x' class='c'>World <a href='https://x'>link</a></p><span>hidden</span>"
        "<script>console.log('a')</script><script type='text/typescript'>let t:number=1;</script></body></html>";

    SourceBundle src = extract_source_bundle(html);
    assert(src.css.find("padding:4px") != std::string::npos);
    assert(src.javascript.find("console.log") != std::string::npos);
    assert(src.typescript.find("number") != std::string::npos);

    std::string rendered = render_page_text(html, 80);
    assert(rendered.find("Hello") != std::string::npos);
    assert(rendered.find("World") != std::string::npos);
    assert(rendered.find("https://x") != std::string::npos);
    assert(rendered.find("hidden") == std::string::npos);

    std::string text;
    std::vector<std::pair<std::string, std::string>> links;
    extract_text_and_links(html, text, links);
    assert(!text.empty());
    assert(!links.empty());

    std::cout << "core_tests passed\n";
    return 0;
}
