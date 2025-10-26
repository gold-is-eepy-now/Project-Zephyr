#pragma once

#include "dom.h"
#include <string>
#include <map>
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
};

class StyleSheet {
public:
    struct Selector {
        std::string tag;
        std::string id;
        std::vector<std::string> classes;
    };
    
    struct Rule {
        Selector selector;
        StyleProperties properties;
    };
    
    void addRule(const Selector& selector, const StyleProperties& properties);
    StyleProperties computeStyle(const ElementPtr& element) const;
    
private:
    std::vector<Rule> rules;
};

// CSS Parser
StyleSheet parse_css(const std::string& css);

} // namespace browser