#include "js_engine.h"
#include <quickjs/quickjs.h>
#include <map>
#include <vector>

namespace browser {

struct EventListener {
    std::string event;
    std::function<void()> callback;
};

static std::map<ElementPtr, std::vector<EventListener>> g_eventListeners;

JSEngine::JSEngine() {
    m_rt = JS_NewRuntime();
    m_ctx = JS_NewContext(m_rt);
    setupGlobalObject();
    setupDOMPrototype();
}

JSEngine::~JSEngine() {
    JS_FreeContext(m_ctx);
    JS_FreeRuntime(m_rt);
}

static JSValue js_getElementById(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    
    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_UNDEFINED;
    
    // Implementation would search the DOM tree for the element
    // For now, return undefined
    JS_FreeCString(ctx, id);
    return JS_UNDEFINED;
}

void JSEngine::setupGlobalObject() {
    JSValue global = JS_GetGlobalObject(m_ctx);
    
    // Create document object
    JSValue document = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, document, "getElementById",
        JS_NewCFunction(m_ctx, js_getElementById, "getElementById", 1));
    
    JS_SetPropertyStr(m_ctx, global, "document", document);
    JS_FreeValue(m_ctx, global);
}

void JSEngine::setupDOMPrototype() {
    // Setup Element prototype with methods like getAttribute, setAttribute, etc.
    // This would be a lot more code in a real implementation
}

JSValue JSEngine::createJSElement(ElementPtr element) {
    JSValue obj = JS_NewObject(m_ctx);
    
    // Add properties and methods
    JS_SetPropertyStr(m_ctx, obj, "tagName",
        JS_NewString(m_ctx, element->tag_name.c_str()));
    
    // Add more properties and methods
    
    return obj;
}

void JSEngine::evaluate(const std::string& script) {
    JSValue val = JS_Eval(m_ctx, script.c_str(), script.length(), "<script>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(m_ctx);
        const char* str = JS_ToCString(m_ctx, exc);
        if (str) {
            // Handle error
            JS_FreeCString(m_ctx, str);
        }
        JS_FreeValue(m_ctx, exc);
    }
    JS_FreeValue(m_ctx, val);
}

void JSEngine::addEventListener(ElementPtr element, const std::string& event, std::function<void()> callback) {
    g_eventListeners[element].push_back({event, callback});
}

void JSEngine::dispatchEvent(ElementPtr element, const std::string& event) {
    auto it = g_eventListeners.find(element);
    if (it != g_eventListeners.end()) {
        for (const auto& listener : it->second) {
            if (listener.event == event) {
                listener.callback();
            }
        }
    }
}

void JSEngine::bindDOM(ElementPtr root) {
    JSValue jsElement = createJSElement(root);
    
    // Store the element in the global object for access from JS
    JSValue global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "rootElement", jsElement);
    JS_FreeValue(m_ctx, global);
    
    // Recursively bind children
    for (const auto& child : root->children) {
        if (auto element = std::dynamic_pointer_cast<Element>(child)) {
            bindDOM(element);
        }
    }
}

}