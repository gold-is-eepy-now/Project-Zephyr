#pragma once

#include "dom.h"
#include <string>
#include <functional>
#include <memory>

// Forward declarations for QuickJS
struct JSRuntime;
struct JSContext;
struct JSValue;

namespace browser {

class JSEngine {
public:
    JSEngine();
    ~JSEngine();
    
    void bindDOM(ElementPtr root);
    void evaluate(const std::string& script);
    
    // Event handling
    void addEventListener(ElementPtr element, const std::string& event, std::function<void()> callback);
    void dispatchEvent(ElementPtr element, const std::string& event);

private:
    JSRuntime* m_rt;
    JSContext* m_ctx;
    
    // DOM bindings
    JSValue createJSElement(ElementPtr element);
    void setupDOMPrototype();
    void setupGlobalObject();
};

}