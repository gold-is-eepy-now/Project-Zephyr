#include "css.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace browser {
namespace {

std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string strip_comments(const std::string& css) {
    std::string out;
    for (size_t i = 0; i < css.size(); ++i) {
        if (i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
            i += 2;
            while (i + 1 < css.size() && !(css[i] == '*' && css[i + 1] == '/')) ++i;
            ++i;
            continue;
        }
        out.push_back(css[i]);
    }
    return out;
}

Color parse_color(const std::string& value) {
    const std::string v = to_lower(trim(value));
    if (v == "black") return Color(0, 0, 0);
    if (v == "white") return Color(255, 255, 255);
    if (v == "red") return Color(255, 0, 0);
    if (v == "green") return Color(0, 255, 0);
    if (v == "blue") return Color(0, 0, 255);
    return Color(0, 0, 0);
}

int parse_dimension(const std::string& value) {
    std::string v = trim(value);
    if (v.empty()) return 0;

    if (const auto px = v.find("px"); px != std::string::npos) v = v.substr(0, px);

    try {
        return std::stoi(v);
    } catch (...) {
        return 0;
    }
}

StyleSheet::Selector parse_selector(const std::string& selector_text) {
    StyleSheet::Selector selector;
    std::string text = trim(selector_text);

    // Support simple descendant selectors: "ancestor target".
    const size_t space = text.find_last_of(" \t\n\r");
    if (space != std::string::npos) {
        selector.ancestor_tag = to_lower(trim(text.substr(0, space)));
        text = trim(text.substr(space + 1));
    }

    size_t i = 0;
    while (i < text.size() && text[i] != '.' && text[i] != '#') ++i;
    selector.tag = to_lower(trim(text.substr(0, i)));

    while (i < text.size()) {
        if (text[i] == '#') {
            size_t start = ++i;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_' || text[i] == '-')) ++i;
            selector.id = text.substr(start, i - start);
        } else if (text[i] == '.') {
            size_t start = ++i;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_' || text[i] == '-')) ++i;
            selector.classes.push_back(text.substr(start, i - start));
        } else {
            ++i;
        }
    }

    return selector;
}

int specificity_of(const StyleSheet::Selector& s) {
    int spec = 0;
    if (!s.id.empty()) spec += 100;
    spec += static_cast<int>(s.classes.size()) * 10;
    if (!s.tag.empty()) spec += 1;
    if (!s.ancestor_tag.empty()) spec += 1;
    return spec;
}

void apply_declaration(StyleProperties& p, const std::string& name, const std::string& value) {
    const std::string n = to_lower(trim(name));
    const std::string v = trim(value);

    if (n == "color") {
        p.color = parse_color(v);
        p.has_color = true;
    } else if (n == "background-color") {
        p.background_color = parse_color(v);
        p.has_background_color = true;
    } else if (n == "font-family") {
        p.font_family = v;
        p.has_font_family = true;
    } else if (n == "font-size") {
        p.font_size = parse_dimension(v);
        p.has_font_size = true;
    } else if (n == "font-weight") {
        p.font_weight = v;
        p.has_font_weight = true;
    } else if (n == "display") {
        p.display = to_lower(v);
        p.has_display = true;
    } else if (n == "margin") {
        const int m = parse_dimension(v);
        p.margin_top = p.margin_right = p.margin_bottom = p.margin_left = m;
        p.has_margin = true;
    } else if (n == "padding") {
        const int pad = parse_dimension(v);
        p.padding_top = p.padding_right = p.padding_bottom = p.padding_left = pad;
        p.has_padding = true;
    }
}

}  // namespace

void StyleSheet::addRule(const Selector& selector, const StyleProperties& properties) {
    rules.push_back({selector, properties, specificity_of(selector), rule_counter++});
}

StyleProperties StyleSheet::computeStyle(const ElementPtr& element) const {
    StyleProperties result;

    auto apply_if_set = [&](const StyleProperties& src) {
        if (src.has_display) result.display = src.display;
        if (src.has_color) result.color = src.color;
        if (src.has_background_color) result.background_color = src.background_color;
        if (src.has_font_family) result.font_family = src.font_family;
        if (src.has_font_size) result.font_size = src.font_size;
        if (src.has_font_weight) result.font_weight = src.font_weight;
        if (src.has_margin) {
            result.margin_top = src.margin_top;
            result.margin_right = src.margin_right;
            result.margin_bottom = src.margin_bottom;
            result.margin_left = src.margin_left;
        }
        if (src.has_padding) {
            result.padding_top = src.padding_top;
            result.padding_right = src.padding_right;
            result.padding_bottom = src.padding_bottom;
            result.padding_left = src.padding_left;
        }
    };

    std::vector<Rule> matching;
    for (const auto& rule : rules) {
        bool matches = true;

        if (!rule.selector.tag.empty() && rule.selector.tag != element->tag_name) matches = false;
        if (matches && !rule.selector.id.empty() && element->getAttribute("id") != rule.selector.id) matches = false;

        if (matches && !rule.selector.classes.empty()) {
            std::istringstream iss(element->getAttribute("class"));
            std::vector<std::string> classes;
            std::string cls;
            while (iss >> cls) classes.push_back(cls);
            for (const auto& required : rule.selector.classes) {
                if (std::find(classes.begin(), classes.end(), required) == classes.end()) {
                    matches = false;
                    break;
                }
            }
        }

        if (matches && !rule.selector.ancestor_tag.empty()) {
            bool found = false;
            auto parent = element->parent.lock();
            while (parent) {
                if (parent->tag_name == rule.selector.ancestor_tag) {
                    found = true;
                    break;
                }
                parent = parent->parent.lock();
            }
            matches = found;
        }

        if (matches) matching.push_back(rule);
    }

    std::sort(matching.begin(), matching.end(), [](const Rule& a, const Rule& b) {
        if (a.specificity != b.specificity) return a.specificity < b.specificity;
        return a.source_order < b.source_order;
    });

    for (const auto& rule : matching) apply_if_set(rule.properties);
    return result;
}

StyleSheet parse_css(const std::string& css) {
    StyleSheet sheet;
    const std::string clean = strip_comments(css);

    size_t pos = 0;
    while (pos < clean.size()) {
        const size_t brace_start = clean.find('{', pos);
        if (brace_start == std::string::npos) break;

        const size_t brace_end = clean.find('}', brace_start + 1);
        if (brace_end == std::string::npos) break;

        const auto selector_list = clean.substr(pos, brace_start - pos);
        StyleProperties properties;

        std::istringstream decls(clean.substr(brace_start + 1, brace_end - brace_start - 1));
        std::string declaration;
        while (std::getline(decls, declaration, ';')) {
            const size_t colon = declaration.find(':');
            if (colon == std::string::npos) continue;
            apply_declaration(properties, declaration.substr(0, colon), declaration.substr(colon + 1));
        }

        std::istringstream selectors(selector_list);
        std::string selector_text;
        while (std::getline(selectors, selector_text, ',')) {
            auto selector = parse_selector(selector_text);
            if (!selector.ancestor_tag.empty() || !selector.tag.empty() || !selector.id.empty() || !selector.classes.empty()) {
                sheet.addRule(selector, properties);
            }
        }

        pos = brace_end + 1;
    }

    return sheet;
}

}  // namespace browser
