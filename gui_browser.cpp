// Minimal Win32 GUI browser using the shared core.
// - Address bar + Go button
// - Read-only multi-line Edit control to show text
// - ListBox for links (double-click to follow)
// Note: synchronous fetch on button click (UI may block briefly). This keeps the example small.

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include "browser_core.h"

static HWND hwndAddress, hwndGo, hwndBack, hwndForward, hwndText, hwndLinks;
static std::vector<std::string> history;
static int hist_index = -1;

void load_url_into_ui(const std::string &url) {
    try {
        HttpResponse r = http_get(url);
        
        // Extract CSS from style tags
        std::string css;
        size_t style_start = r.body.find("<style>");
        while (style_start != std::string::npos) {
            size_t style_end = r.body.find("</style>", style_start);
            if (style_end != std::string::npos) {
                css += r.body.substr(style_start + 7, style_end - (style_start + 7));
                style_start = r.body.find("<style>", style_end);
            } else {
                break;
            }
        }
        
        // Parse document with DOM and CSS
        browser::RenderContext ctx = browser::parse_document(r.body, css);
        
        // Extract text and links for now (we'll add proper rendering later)
        std::string plain;
        std::vector<std::pair<std::string,std::string>> links;
        extract_text_and_links(r.body, plain, links);

        // update text control
        SetWindowTextA(hwndText, plain.c_str());

        // update listbox
        SendMessage(hwndLinks, LB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < links.size(); ++i) {
            std::ostringstream item;
            item << (i+1) << ": " << links[i].first << " -> " << links[i].second;
            SendMessageA(hwndLinks, LB_ADDSTRING, 0, (LPARAM)item.str().c_str());
        }

        // push history
        if (hist_index + 1 < (int)history.size()) history.resize(hist_index+1);
        history.push_back(url);
        hist_index = (int)history.size() - 1;

    } catch (const std::exception &ex) {
        SetWindowTextA(hwndText, ex.what());
    }
}

static std::string get_address_text() {
    int len = GetWindowTextLengthW(hwndAddress);
    std::wstring w;
    w.resize(len);
    GetWindowTextW(hwndAddress, &w[0], len+1);
    std::string s;
    s.assign(w.begin(), w.end());
    return s;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hwndAddress = CreateWindowW(L"EDIT", L"http://example.com", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT, 10,10,420,24, hWnd, NULL, NULL, NULL);
        hwndGo = CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 440, 10, 50, 24, hWnd, (HMENU)1001, NULL, NULL);
        hwndBack = CreateWindowW(L"BUTTON", L"◀", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 500, 10, 36, 24, hWnd, (HMENU)1002, NULL, NULL);
        hwndForward = CreateWindowW(L"BUTTON", L"▶", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 540, 10, 36, 24, hWnd, (HMENU)1003, NULL, NULL);

        hwndText = CreateWindowW(L"EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 10, 50, 560, 360, hWnd, NULL, NULL, NULL);
        hwndLinks = CreateWindowW(L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 580, 50, 260, 360, hWnd, NULL, NULL, NULL);
        break; }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 1001) { // Go
            std::string url = get_address_text();
            if (url.find("://") == std::string::npos) url = "http://" + url;
            load_url_into_ui(url);
        } else if (id == 1002) { // back
            if (hist_index > 0) {
                hist_index -= 1;
                load_url_into_ui(history[hist_index]);
            }
        } else if (id == 1003) { // forward
            if (hist_index + 1 < (int)history.size()) {
                hist_index += 1;
                load_url_into_ui(history[hist_index]);
            }
        } else if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == hwndLinks) {
            int sel = (int)SendMessage(hwndLinks, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                char buf[1024];
                SendMessageA(hwndLinks, LB_GETTEXT, sel, (LPARAM)buf);
                // parse stored item 'N: text -> href' to extract href
                std::string item(buf);
                auto pos = item.rfind(" -> ");
                if (pos != std::string::npos) {
                    std::string href = item.substr(pos + 4);
                    std::string base = get_address_text();
                    std::string next;
                    if (href.rfind("http://", 0) == 0) next = href;
                    else {
                        UrlParts parts; parse_url(base, parts);
                        if (!href.empty() && href[0] == '/') next = parts.scheme + "://" + parts.host + href;
                        else {
                            std::string b = parts.path;
                            auto p = b.rfind('/');
                            if (p == std::string::npos) b = "/"; else b = b.substr(0, p+1);
                            next = parts.scheme + "://" + parts.host + b + href;
                        }
                    }
                    SetWindowTextA(hwndAddress, next.c_str());
                    load_url_into_ui(next);
                }
            }
        }
        break; }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[]  = L"MiniBrowserClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Mini GUI Browser", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 860, 480, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // initial load
    SetWindowTextW(hwndAddress, L"http://example.com");
    load_url_into_ui("http://example.com");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
