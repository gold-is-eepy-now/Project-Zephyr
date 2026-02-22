#pragma once

#include "dom.h"
#include <string>
#include <vector>

namespace browser {

struct Color {
    unsigned char r, g, b, a;
    Color() : r(0), g(0), b(0), a(255) {}
    Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        : r(r), g(g), b(b), a(a) {}
};

struct StyleProperties {
    std::string display = "block";
    Color color{0, 0, 0};
    Color background_color{255, 255, 255};
    std::string font_family = "Arial";
    int font_size = 16;
    std::string font_weight = "normal";
    int margin_top = 0;
    int margin_right = 0;
    int margin_bottom = 0;
    int margin_left = 0;
    int padding_top = 0;
    int padding_right = 0;
    int padding_bottom = 0;
    int padding_left = 0;

    bool has_display = false;
    bool has_color = false;
    bool has_background_color = false;
    bool has_font_family = false;
    bool has_font_size = false;
    bool has_font_weight = false;
    bool has_margin = false;
    bool has_padding = false;
};

class StyleSheet {
public:
    struct Selector {
        std::string ancestor_tag;
        std::string tag;
        std::string id;
        std::vector<std::string> classes;
    };

    struct Rule {
        Selector selector;
        StyleProperties properties;
        int specificity = 0;
        size_t source_order = 0;
    };

    void addRule(const Selector& selector, const StyleProperties& properties);
    StyleProperties computeStyle(const ElementPtr& element) const;

private:
    std::vector<Rule> rules;
    size_t rule_counter = 0;
};

StyleSheet parse_css(const std::string& css);

} // namespace browser
