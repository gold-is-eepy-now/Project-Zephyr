#include "dom.h"
#include <stack>
#include <cctype>

namespace browser {

void Element::appendChild(NodePtr child) {
    children.push_back(child);
    if (auto element = std::dynamic_pointer_cast<Element>(child)) {
        element->parent = shared_from_this();
    }
}

std::string Element::getAttribute(const std::string& name) const {
    auto it = attributes.find(name);
    return it != attributes.end() ? it->second : "";
}

void Element::setAttribute(const std::string& name, const std::string& value) {
    attributes[name] = value;
}

ElementPtr parse_html(const std::string& html) {
    auto root = Element::create("html");
    auto current = root;
    std::stack<ElementPtr> elements;
    elements.push(root);
    
    size_t pos = 0;
    while (pos < html.length()) {
        if (html[pos] == '<') {
            if (pos + 1 < html.length() && html[pos + 1] == '/') {
                // Closing tag
                size_t end = html.find('>', pos);
                if (end != std::string::npos) {
                    if (elements.size() > 1) {
                        elements.pop();
                        current = elements.top();
                    }
                    pos = end + 1;
                }
            } else {
                // Opening tag
                size_t end = html.find('>', pos);
                if (end != std::string::npos) {
                    std::string tag = html.substr(pos + 1, end - pos - 1);
                    size_t space = tag.find(' ');
                    std::string tag_name = space != std::string::npos ? tag.substr(0, space) : tag;
                    
                    auto element = Element::create(tag_name);
                    
                    // Parse attributes
                    if (space != std::string::npos) {
                        std::string attrs = tag.substr(space + 1);
                        size_t attr_pos = 0;
                        while (attr_pos < attrs.length()) {
                            while (attr_pos < attrs.length() && std::isspace(attrs[attr_pos])) attr_pos++;
                            size_t equals = attrs.find('=', attr_pos);
                            if (equals != std::string::npos) {
                                std::string name = attrs.substr(attr_pos, equals - attr_pos);
                                attr_pos = equals + 1;
                                char quote = attrs[attr_pos];
                                if (quote == '"' || quote == '\'') {
                                    attr_pos++;
                                    size_t quote_end = attrs.find(quote, attr_pos);
                                    if (quote_end != std::string::npos) {
                                        std::string value = attrs.substr(attr_pos, quote_end - attr_pos);
                                        element->setAttribute(name, value);
                                        attr_pos = quote_end + 1;
                                    }
                                }
                            } else {
                                break;
                            }
                        }
                    }
                    
                    current->appendChild(element);
                    if (tag_name != "br" && tag_name != "img" && tag_name != "hr" && tag_name != "input") {
                        elements.push(element);
                        current = element;
                    }
                    pos = end + 1;
                }
            }
        } else {
            // Text content
            size_t end = html.find('<', pos);
            if (end == std::string::npos) end = html.length();
            std::string text = html.substr(pos, end - pos);
            if (!text.empty() && text.find_first_not_of(" \t\n\r") != std::string::npos) {
                current->appendChild(TextNode::create(text));
            }
            pos = end;
        }
    }
    
    return root;
}

} // namespace browser