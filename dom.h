#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace browser {

enum class NodeType { ELEMENT, TEXT, COMMENT };

class Node;
class Element;
class TextNode;

using NodePtr = std::shared_ptr<Node>;
using ElementPtr = std::shared_ptr<Element>;

class Node {
public:
    explicit Node(NodeType t) : type(t) {}
    virtual ~Node() = default;

    NodeType type;
    std::weak_ptr<Element> parent;
};

class Element : public Node, public std::enable_shared_from_this<Element> {
public:
    explicit Element(const std::string& name);

    static ElementPtr create(const std::string& name);

    std::string tag_name;
    std::map<std::string, std::string> attributes;
    std::vector<NodePtr> children;

    void appendChild(const NodePtr& child);
    std::string getAttribute(const std::string& key) const;
    void setAttribute(const std::string& key, const std::string& value);
};

class TextNode : public Node {
public:
    explicit TextNode(const std::string& t);

    static std::shared_ptr<TextNode> create(const std::string& t);

    std::string text;
};

ElementPtr parse_html(const std::string& html);

}  // namespace browser
