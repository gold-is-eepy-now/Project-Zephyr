#include "browser_core.h"
#include "css.h"

#include <cassert>
#include <iostream>

int main() {
    UrlParts parts;
    assert(parse_url("http://example.com/a/b", parts));
    assert(parts.host == "example.com");
    assert(parse_url("https://example.com/a/b", parts));
    assert(parts.port == 443);

    assert(resolve_url("http://example.com/a/b", "../c") == "http://example.com/c");
    assert(resolve_url("https://example.com/a/b", "../c") == "https://example.com/c");
    assert(resolve_url("http://example.com/a/b", "javascript:alert(1)").empty());

    std::string text;
    std::vector<std::pair<std::string, std::string>> links;
    extract_text_and_links("<html><body><a href='https://x'>A &#66; &amp; C</a><script>alert(1)</script></body></html>", text, links);
    assert(!text.empty());
    assert(text.find("A B & C") != std::string::npos);
    assert(links.size() == 1);

    auto sheet = browser::parse_css("article a.btn{padding:4px;} #id{color:red;} .btn{font-size:20px;}");
    auto article = browser::Element::create("article");
    auto el = browser::Element::create("a");
    el->setAttribute("id", "id");
    el->setAttribute("class", "btn");
    article->appendChild(el);

    auto style = sheet.computeStyle(el);
    assert(style.color.r == 255);
    assert(style.font_size == 20);
    assert(style.padding_top == 4);

    std::cout << "core_tests passed\n";
    return 0;
}
