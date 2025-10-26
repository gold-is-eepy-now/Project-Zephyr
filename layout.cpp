#include "layout.h"

namespace browser {

void LayoutEngine::measureText(const std::string& text, const StyleProperties& style, int& width, int& height) {
    HFONT hFont = CreateFontA(
        style.font_size, 0, 0, 0,
        style.font_weight == "bold" ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        style.font_family.c_str()
    );
    
    HFONT hOldFont = (HFONT)SelectObject(m_hdc, hFont);
    SIZE size;
    GetTextExtentPoint32A(m_hdc, text.c_str(), (int)text.length(), &size);
    SelectObject(m_hdc, hOldFont);
    DeleteObject(hFont);
    
    width = size.cx;
    height = size.cy;
}

RenderBox LayoutEngine::computeLayout(ElementPtr root, const StyleSheet& stylesheet, int containerWidth) {
    RenderBox box;
    box.element = root;
    box.style = stylesheet.computeStyle(root);
    
    int x = box.style.margin_left;
    int y = box.style.margin_top;
    int maxWidth = 0;
    int totalHeight = 0;
    
    for (const auto& child : root->children) {
        if (auto element = std::dynamic_pointer_cast<Element>(child)) {
            auto childBox = computeLayout(element, stylesheet, containerWidth - box.style.margin_left - box.style.margin_right);
            childBox.bounds.x = x + box.style.padding_left;
            childBox.bounds.y = y + totalHeight + box.style.padding_top;
            box.children.push_back(childBox);
            
            maxWidth = std::max(maxWidth, childBox.bounds.width);
            totalHeight += childBox.bounds.height + box.style.padding_top + box.style.padding_bottom;
        } else if (auto text = std::dynamic_pointer_cast<TextNode>(child)) {
            int textWidth, textHeight;
            measureText(text->text, box.style, textWidth, textHeight);
            
            RenderBox textBox;
            textBox.bounds = {x + box.style.padding_left, y + totalHeight + box.style.padding_top, textWidth, textHeight};
            box.children.push_back(textBox);
            
            maxWidth = std::max(maxWidth, textWidth);
            totalHeight += textHeight + box.style.padding_top + box.style.padding_bottom;
        }
    }
    
    box.bounds = {
        x,
        y,
        maxWidth + box.style.padding_left + box.style.padding_right + box.style.margin_left + box.style.margin_right,
        totalHeight + box.style.margin_top + box.style.margin_bottom
    };
    
    return box;
}

void LayoutEngine::renderText(const std::string& text, const StyleProperties& style, int x, int y) {
    HFONT hFont = CreateFontA(
        style.font_size, 0, 0, 0,
        style.font_weight == "bold" ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        style.font_family.c_str()
    );
    
    HFONT hOldFont = (HFONT)SelectObject(m_hdc, hFont);
    SetTextColor(m_hdc, RGB(style.color.r, style.color.g, style.color.b));
    SetBkMode(m_hdc, TRANSPARENT);
    TextOutA(m_hdc, x, y, text.c_str(), (int)text.length());
    SelectObject(m_hdc, hOldFont);
    DeleteObject(hFont);
}

void LayoutEngine::render(const RenderBox& box) {
    // Draw background if specified
    if (box.style.background_color.a > 0) {
        HBRUSH hBrush = CreateSolidBrush(RGB(
            box.style.background_color.r,
            box.style.background_color.g,
            box.style.background_color.b
        ));
        RECT rect = {
            box.bounds.x,
            box.bounds.y,
            box.bounds.x + box.bounds.width,
            box.bounds.y + box.bounds.height
        };
        FillRect(m_hdc, &rect, hBrush);
        DeleteObject(hBrush);
    }
    
    // Render text content
    if (box.element && box.element->type == NodeType::TEXT) {
        auto text = std::dynamic_pointer_cast<TextNode>(box.element);
        if (text) {
            renderText(text->text, box.style, box.bounds.x, box.bounds.y);
        }
    }
    
    // Render children
    for (const auto& child : box.children) {
        render(child);
    }
}

}