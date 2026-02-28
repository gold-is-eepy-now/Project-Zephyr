#pragma once

#include "dom.h"

#include <string>
#include <vector>

namespace browser {

struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;
};

struct StyleProperties {
    std::string display;
    Color color;
    int font_size = 16;
    int padding_top = 0;
    int padding_right = 0;
    int padding_bottom = 0;
    int padding_left = 0;

    bool has_display = false;
    bool has_color = false;
    bool has_font_size = false;
    bool has_padding = false;
};

struct Selector {
    std::string ancestor_tag;
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
};

class StyleSheet {
public:
    void addRule(const Selector& selector, const StyleProperties& properties);
    StyleProperties computeStyle(const ElementPtr& element) const;

private:
    struct Rule {
        Selector selector;
        StyleProperties properties;
        int specificity = 0;
        size_t order = 0;
    };

    std::vector<Rule> rules_;
    size_t next_order_ = 0;
};

StyleSheet parse_css(const std::string& css_text);

}  // namespace browser
