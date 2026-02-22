#include "dom.h"

#include <algorithm>
#include <cctype>
#include <stack>

namespace browser {
namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool is_void_tag(const std::string& tag) {
    static const std::vector<std::string> kVoidTags = {
        "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"};
    return std::find(kVoidTags.begin(), kVoidTags.end(), tag) != kVoidTags.end();
}

bool is_raw_text_tag(const std::string& tag) {
    return tag == "script" || tag == "style";
}

void parse_attributes(const std::string& src, ElementPtr& element) {
    size_t pos = 0;
    while (pos < src.size()) {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) ++pos;
        if (pos >= src.size()) break;

        size_t name_end = pos;
        while (name_end < src.size() && !std::isspace(static_cast<unsigned char>(src[name_end])) && src[name_end] != '=') ++name_end;
        const std::string name = to_lower(src.substr(pos, name_end - pos));
        pos = name_end;

        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) ++pos;
        if (pos >= src.size() || src[pos] != '=') {
            element->setAttribute(name, "true");
            continue;
        }

        ++pos;
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) ++pos;
        if (pos >= src.size()) break;

        std::string value;
        if (src[pos] == '"' || src[pos] == '\'') {
            const char quote = src[pos++];
            const size_t end = src.find(quote, pos);
            value = src.substr(pos, end - pos);
            pos = (end == std::string::npos) ? src.size() : end + 1;
        } else {
            size_t end = pos;
            while (end < src.size() && !std::isspace(static_cast<unsigned char>(src[end]))) ++end;
            value = src.substr(pos, end - pos);
            pos = end;
        }

        element->setAttribute(name, value);
    }
}

}  // namespace

void Element::appendChild(NodePtr child) {
    children.push_back(child);
    if (auto element = std::dynamic_pointer_cast<Element>(child)) {
        element->parent = shared_from_this();
    }
}

std::string Element::getAttribute(const std::string& name) const {
    const auto it = attributes.find(name);
    return it != attributes.end() ? it->second : "";
}

void Element::setAttribute(const std::string& name, const std::string& value) { attributes[name] = value; }

ElementPtr parse_html(const std::string& html) {
    auto root = Element::create("html");
    std::stack<ElementPtr> stack;
    stack.push(root);

    size_t pos = 0;
    while (pos < html.size()) {
        if (html[pos] != '<') {
            const size_t end = html.find('<', pos);
            const std::string text = html.substr(pos, end - pos);
            if (text.find_first_not_of(" \t\n\r") != std::string::npos) {
                stack.top()->appendChild(TextNode::create(text));
            }
            pos = (end == std::string::npos) ? html.size() : end;
            continue;
        }

        if (pos + 3 < html.size() && html.compare(pos, 4, "<!--") == 0) {
            const size_t comment_end = html.find("-->", pos + 4);
            pos = (comment_end == std::string::npos) ? html.size() : comment_end + 3;
            continue;
        }

        const size_t end = html.find('>', pos + 1);
        if (end == std::string::npos) break;

        std::string tag_raw = html.substr(pos + 1, end - pos - 1);
        bool self_closing = false;
        if (!tag_raw.empty() && tag_raw.back() == '/') {
            self_closing = true;
            tag_raw.pop_back();
        }

        if (!tag_raw.empty() && tag_raw[0] == '!') {
            pos = end + 1;
            continue;
        }

        if (!tag_raw.empty() && tag_raw[0] == '/') {
            if (stack.size() > 1) stack.pop();
            pos = end + 1;
            continue;
        }

        const size_t space = tag_raw.find_first_of(" \t\n\r");
        std::string tag_name = to_lower(tag_raw.substr(0, space));
        auto element = Element::create(tag_name);

        if (space != std::string::npos) parse_attributes(tag_raw.substr(space + 1), element);

        stack.top()->appendChild(element);

        if (is_raw_text_tag(tag_name)) {
            const std::string close_tag = "</" + tag_name + ">";
            const std::string lower_html = to_lower(html);
            const size_t close = lower_html.find(close_tag, end + 1);
            if (close != std::string::npos && close > end + 1) {
                element->appendChild(TextNode::create(html.substr(end + 1, close - end - 1)));
                pos = close + close_tag.size();
            } else {
                pos = html.size();
            }
            continue;
        }

        if (!self_closing && !is_void_tag(tag_name)) stack.push(element);
        pos = end + 1;
    }

    return root;
}

}  // namespace browser
