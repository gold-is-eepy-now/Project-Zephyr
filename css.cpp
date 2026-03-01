#include "css.h"

#include <algorithm>
#include <cctype>
#include <sstream>

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

int parse_px(const std::string& s) {
    std::string n;
    for (char c : s) {
        if ((c >= '0' && c <= '9') || c == '-') n.push_back(c);
        else break;
    }
    if (n.empty() || n == "-") return 0;
    return std::stoi(n);
}

Color parse_color(const std::string& s) {
    const std::string v = lower(trim(s));
    if (v == "red") return {255, 0, 0, 255};
    if (v == "green") return {0, 128, 0, 255};
    if (v == "blue") return {0, 0, 255, 255};
    if (v == "black") return {0, 0, 0, 255};
    if (v == "white") return {255, 255, 255, 255};
    if (v.size() == 7 && v[0] == '#') {
        auto h = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            return 0;
        };
        return {h(v[1]) * 16 + h(v[2]), h(v[3]) * 16 + h(v[4]), h(v[5]) * 16 + h(v[6]), 255};
    }
    return {};
}

std::string strip_comments(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size();) {
        if (i + 1 < in.size() && in[i] == '/' && in[i + 1] == '*') {
            size_t end = in.find("*/", i + 2);
            if (end == std::string::npos) break;
            i = end + 2;
            continue;
        }
        out.push_back(in[i++]);
    }
    return out;
}

Selector parse_selector(std::string text) {
    Selector s;
    text = trim(lower(text));

    size_t space = text.find(' ');
    if (space != std::string::npos) {
        s.ancestor_tag = trim(text.substr(0, space));
        text = trim(text.substr(space + 1));
    }

    std::string token;
    for (size_t i = 0; i <= text.size(); ++i) {
        const char c = (i < text.size()) ? text[i] : '\0';
        if (c == '#' || c == '.' || c == '\0') {
            if (!token.empty() && s.tag.empty()) s.tag = token;
            token.clear();
            if (c == '#') {
                size_t j = i + 1;
                while (j < text.size() && text[j] != '.' && text[j] != '#') ++j;
                s.id = text.substr(i + 1, j - (i + 1));
                i = j - 1;
            } else if (c == '.') {
                size_t j = i + 1;
                while (j < text.size() && text[j] != '.' && text[j] != '#') ++j;
                s.classes.push_back(text.substr(i + 1, j - (i + 1)));
                i = j - 1;
            }
        } else {
            token.push_back(c);
        }
    }

    return s;
}

int specificity_of(const Selector& s) {
    int sp = 0;
    if (!s.id.empty()) sp += 100;
    sp += static_cast<int>(s.classes.size()) * 10;
    if (!s.tag.empty()) sp += 1;
    if (!s.ancestor_tag.empty()) sp += 1;
    return sp;
}

bool matches(const Selector& s, const ElementPtr& el) {
    if (!s.tag.empty() && s.tag != el->tag_name) return false;
    if (!s.id.empty() && el->getAttribute("id") != s.id) return false;

    if (!s.classes.empty()) {
        std::istringstream iss(el->getAttribute("class"));
        std::vector<std::string> cls;
        std::string c;
        while (iss >> c) cls.push_back(c);
        for (const auto& need : s.classes) {
            if (std::find(cls.begin(), cls.end(), need) == cls.end()) return false;
        }
    }

    if (!s.ancestor_tag.empty()) {
        auto p = el->parent.lock();
        bool found = false;
        while (p) {
            if (p->tag_name == s.ancestor_tag) {
                found = true;
                break;
            }
            p = p->parent.lock();
        }
        if (!found) return false;
    }

    return true;
}

void apply_decl(StyleProperties& p, std::string name, std::string value) {
    name = lower(trim(name));
    value = trim(value);
    if (name == "display") {
        p.display = lower(value);
        p.has_display = true;
    } else if (name == "color") {
        p.color = parse_color(value);
        p.has_color = true;
    } else if (name == "font-size") {
        p.font_size = parse_px(value);
        p.has_font_size = true;
    } else if (name == "padding") {
        const int v = parse_px(value);
        p.padding_top = p.padding_right = p.padding_bottom = p.padding_left = v;
        p.has_padding = true;
    }
}

}  // namespace

void StyleSheet::addRule(const Selector& selector, const StyleProperties& properties) {
    rules_.push_back({selector, properties, specificity_of(selector), next_order_++});
}

StyleProperties StyleSheet::computeStyle(const ElementPtr& element) const {
    StyleProperties out;
    std::vector<Rule> applicable;
    for (const auto& r : rules_) {
        if (matches(r.selector, element)) applicable.push_back(r);
    }

    std::sort(applicable.begin(), applicable.end(), [](const Rule& a, const Rule& b) {
        if (a.specificity != b.specificity) return a.specificity < b.specificity;
        return a.order < b.order;
    });

    for (const auto& r : applicable) {
        if (r.properties.has_display) {
            out.display = r.properties.display;
            out.has_display = true;
        }
        if (r.properties.has_color) {
            out.color = r.properties.color;
            out.has_color = true;
        }
        if (r.properties.has_font_size) {
            out.font_size = r.properties.font_size;
            out.has_font_size = true;
        }
        if (r.properties.has_padding) {
            out.padding_top = r.properties.padding_top;
            out.padding_right = r.properties.padding_right;
            out.padding_bottom = r.properties.padding_bottom;
            out.padding_left = r.properties.padding_left;
            out.has_padding = true;
        }
    }

    return out;
}

StyleSheet parse_css(const std::string& css_text) {
    StyleSheet sheet;
    const std::string clean = strip_comments(css_text);

    size_t pos = 0;
    while (pos < clean.size()) {
        const size_t open = clean.find('{', pos);
        if (open == std::string::npos) break;
        const size_t close = clean.find('}', open + 1);
        if (close == std::string::npos) break;

        const std::string selectors = clean.substr(pos, open - pos);
        const std::string body = clean.substr(open + 1, close - open - 1);

        StyleProperties props;
        std::istringstream decls(body);
        std::string d;
        while (std::getline(decls, d, ';')) {
            size_t colon = d.find(':');
            if (colon == std::string::npos) continue;
            apply_decl(props, d.substr(0, colon), d.substr(colon + 1));
        }

        std::istringstream sels(selectors);
        std::string s;
        while (std::getline(sels, s, ',')) {
            Selector parsed = parse_selector(s);
            if (!parsed.tag.empty() || !parsed.id.empty() || !parsed.classes.empty() || !parsed.ancestor_tag.empty()) {
                sheet.addRule(parsed, props);
            }
        }

        pos = close + 1;
    }

    return sheet;
}

}  // namespace browser
