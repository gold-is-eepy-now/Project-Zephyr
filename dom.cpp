#include "dom.h"

#include <algorithm>
#include <cctype>
#include <stack>
#include <unordered_set>

namespace browser {
namespace {

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

bool is_void(const std::string& tag) {
    static const std::unordered_set<std::string> kVoid = {
        "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"};
    return kVoid.find(tag) != kVoid.end();
}

void parse_attributes(const std::string& src, ElementPtr& el) {
    size_t i = 0;
    while (i < src.size()) {
        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
        if (i >= src.size()) break;

        size_t name_end = i;
        while (name_end < src.size() && !std::isspace(static_cast<unsigned char>(src[name_end])) && src[name_end] != '=') ++name_end;
        std::string key = lower(src.substr(i, name_end - i));
        i = name_end;

        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
        if (i >= src.size() || src[i] != '=') {
            if (!key.empty()) el->setAttribute(key, "true");
            continue;
        }

        ++i;
        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
        if (i >= src.size()) break;

        std::string value;
        if (src[i] == '"' || src[i] == '\'') {
            const char q = src[i++];
            size_t end = src.find(q, i);
            if (end == std::string::npos) end = src.size();
            value = src.substr(i, end - i);
            i = (end < src.size()) ? end + 1 : end;
        } else {
            size_t end = i;
            while (end < src.size() && !std::isspace(static_cast<unsigned char>(src[end]))) ++end;
            value = src.substr(i, end - i);
            i = end;
        }

        if (!key.empty()) el->setAttribute(key, trim(value));
    }
}

}  // namespace

Element::Element(const std::string& name) : Node(NodeType::ELEMENT), tag_name(lower(name)) {}
ElementPtr Element::create(const std::string& name) { return std::make_shared<Element>(name); }

void Element::appendChild(const NodePtr& child) {
    children.push_back(child);
    child->parent = shared_from_this();
}

std::string Element::getAttribute(const std::string& key) const {
    auto it = attributes.find(lower(key));
    return it == attributes.end() ? "" : it->second;
}

void Element::setAttribute(const std::string& key, const std::string& value) { attributes[lower(key)] = value; }

TextNode::TextNode(const std::string& t) : Node(NodeType::TEXT), text(t) {}
std::shared_ptr<TextNode> TextNode::create(const std::string& t) { return std::make_shared<TextNode>(t); }

ElementPtr parse_html(const std::string& html) {
    auto root = Element::create("document");
    std::stack<ElementPtr> st;
    st.push(root);

    size_t i = 0;
    while (i < html.size()) {
        if (html[i] != '<') {
            size_t end = html.find('<', i);
            if (end == std::string::npos) end = html.size();
            std::string text = html.substr(i, end - i);
            if (text.find_first_not_of(" \t\r\n") != std::string::npos) st.top()->appendChild(TextNode::create(text));
            i = end;
            continue;
        }

        if (html.compare(i, 4, "<!--") == 0) {
            size_t end = html.find("-->", i + 4);
            i = (end == std::string::npos) ? html.size() : end + 3;
            continue;
        }

        size_t end = html.find('>', i + 1);
        if (end == std::string::npos) break;

        std::string raw = trim(html.substr(i + 1, end - i - 1));
        i = end + 1;
        if (raw.empty() || raw[0] == '!') continue;

        if (raw[0] == '/') {
            if (st.size() > 1) st.pop();
            continue;
        }

        bool self_close = false;
        if (!raw.empty() && raw.back() == '/') {
            self_close = true;
            raw.pop_back();
            raw = trim(raw);
        }

        size_t sp = raw.find_first_of(" \t\r\n");
        std::string tag = (sp == std::string::npos) ? raw : raw.substr(0, sp);
        auto el = Element::create(tag);
        if (sp != std::string::npos) parse_attributes(raw.substr(sp + 1), el);
        st.top()->appendChild(el);

        if (is_void(el->tag_name) || self_close) continue;

        if (el->tag_name == "script" || el->tag_name == "style") {
            const std::string close_tag = "</" + el->tag_name + ">";
            std::string lower_html = lower(html);
            size_t close_pos = lower_html.find(close_tag, i);
            if (close_pos == std::string::npos) close_pos = html.size();
            if (close_pos > i) el->appendChild(TextNode::create(html.substr(i, close_pos - i)));
            i = (close_pos >= html.size()) ? close_pos : close_pos + close_tag.size();
            continue;
        }

        st.push(el);
    }

    return root;
}

}  // namespace browser
