#include "css.h"
#include <cctype>
#include <sstream>
#include <algorithm>

namespace browser {

void StyleSheet::addRule(const Selector& selector, const StyleProperties& properties) {
    rules.push_back({selector, properties});
}

StyleProperties StyleSheet::computeStyle(const ElementPtr& element) const {
    StyleProperties result;
    
    // Apply matching rules
    for (const auto& rule : rules) {
        bool matches = true;
        
        // Check tag name
        if (!rule.selector.tag.empty() && rule.selector.tag != element->tag_name) {
            matches = false;
        }
        
        // Check ID
        if (!rule.selector.id.empty()) {
            auto id = element->getAttribute("id");
            if (id != rule.selector.id) {
                matches = false;
            }
        }
        
        // Check classes
        if (!rule.selector.classes.empty()) {
            auto class_attr = element->getAttribute("class");
            std::istringstream iss(class_attr);
            std::vector<std::string> classes;
            std::string cls;
            while (iss >> cls) {
                classes.push_back(cls);
            }
            
            for (const auto& required_class : rule.selector.classes) {
                if (std::find(classes.begin(), classes.end(), required_class) == classes.end()) {
                    matches = false;
                    break;
                }
            }
        }
        
        if (matches) {
            // Apply the matching rule's properties
            result = rule.properties;
        }
    }
    
    return result;
}

// Helper function to parse color values
Color parse_color(const std::string& value) {
    if (value == "black") return Color(0, 0, 0);
    if (value == "white") return Color(255, 255, 255);
    if (value == "red") return Color(255, 0, 0);
    if (value == "green") return Color(0, 255, 0);
    if (value == "blue") return Color(0, 0, 255);
    return Color(0, 0, 0); // Default to black
}

// Helper function to parse dimension values
int parse_dimension(const std::string& value) {
    if (value.empty()) return 0;
    try {
        size_t px_pos = value.find("px");
        if (px_pos != std::string::npos) {
            return std::stoi(value.substr(0, px_pos));
        }
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

StyleSheet parse_css(const std::string& css) {
    StyleSheet sheet;
    size_t pos = 0;
    
    while (pos < css.length()) {
        // Skip whitespace
        while (pos < css.length() && std::isspace(css[pos])) pos++;
        if (pos >= css.length()) break;
        
        // Parse selector
        StyleSheet::Selector selector;
        std::string selector_text;
        size_t brace_start = css.find('{', pos);
        if (brace_start == std::string::npos) break;
        
        selector_text = css.substr(pos, brace_start - pos);
        // Simple selector parsing (just tag names for now)
        selector.tag = selector_text;
        
        // Find closing brace
        size_t brace_end = css.find('}', brace_start);
        if (brace_end == std::string::npos) break;
        
        // Parse properties
        StyleProperties properties;
        std::string props = css.substr(brace_start + 1, brace_end - brace_start - 1);
        std::istringstream iss(props);
        std::string prop;
        
        while (std::getline(iss, prop, ';')) {
            size_t colon = prop.find(':');
            if (colon != std::string::npos) {
                std::string name = prop.substr(0, colon);
                std::string value = prop.substr(colon + 1);
                
                // Trim whitespace
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Set property values
                if (name == "color") {
                    properties.color = parse_color(value);
                }
                else if (name == "background-color") {
                    properties.background_color = parse_color(value);
                }
                else if (name == "font-family") {
                    properties.font_family = value;
                }
                else if (name == "font-size") {
                    properties.font_size = parse_dimension(value);
                }
                else if (name == "font-weight") {
                    properties.font_weight = value;
                }
                else if (name == "margin") {
                    int margin = parse_dimension(value);
                    properties.margin_top = properties.margin_right = 
                    properties.margin_bottom = properties.margin_left = margin;
                }
                else if (name == "padding") {
                    int padding = parse_dimension(value);
                    properties.padding_top = properties.padding_right = 
                    properties.padding_bottom = properties.padding_left = padding;
                }
            }
        }
        
        sheet.addRule(selector, properties);
        pos = brace_end + 1;
    }
    
    return sheet;
}

} // namespace browser