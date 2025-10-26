#pragma once

#include "dom.h"
#include "css.h"
#include <windows.h>

namespace browser {

struct Rect {
    int x, y, width, height;
};

struct RenderBox {
    ElementPtr element;
    StyleProperties style;
    Rect bounds;
    std::vector<RenderBox> children;
};

class LayoutEngine {
public:
    LayoutEngine(HDC hdc) : m_hdc(hdc) {}
    
    RenderBox computeLayout(ElementPtr root, const StyleSheet& stylesheet, int containerWidth);
    void render(const RenderBox& box);

private:
    HDC m_hdc;
    void measureText(const std::string& text, const StyleProperties& style, int& width, int& height);
    void renderText(const std::string& text, const StyleProperties& style, int x, int y);
};

}