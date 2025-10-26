#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace browser {

enum class NodeType {
    ELEMENT,
    TEXT,
    COMMENT
};

class Node;
class Element;
class TextNode;

using NodePtr = std::shared_ptr<Node>;
using ElementPtr = std::shared_ptr<Element>;

class Node {
public:
    NodeType type;
    std::weak_ptr<Element> parent;
    virtual ~Node() = default;
    
protected:
    Node(NodeType t) : type(t) {}
};

class Element : public Node, public std::enable_shared_from_this<Element> {
public:
    std::string tag_name;
    std::map<std::string, std::string> attributes;
    std::vector<NodePtr> children;
    
    static ElementPtr create(const std::string& tag) {
        return std::shared_ptr<Element>(new Element(tag));
    }
    
    void appendChild(NodePtr child);
    std::string getAttribute(const std::string& name) const;
    void setAttribute(const std::string& name, const std::string& value);
    
private:
    Element(const std::string& tag) : Node(NodeType::ELEMENT), tag_name(tag) {}
};

class TextNode : public Node {
public:
    std::string text;
    
    static std::shared_ptr<TextNode> create(const std::string& content) {
        return std::shared_ptr<TextNode>(new TextNode(content));
    }
    
private:
    TextNode(const std::string& content) : Node(NodeType::TEXT), text(content) {}
};

// DOM Parser
ElementPtr parse_html(const std::string& html);

} // namespace browser