// Minimal Win32 GUI browser using the shared core.
// - Address bar + Go button
// - Read-only multi-line Edit control to show extracted page text
// - ListBox for links (double-click to follow)

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>

#include <sstream>
#include <string>
#include <vector>

#include "browser_core.h"

static HWND hwndAddress;
static HWND hwndSource;

static std::vector<std::string> history;
static int history_index = -1;

static std::string build_source_text(const SourceBundle& src) {
    std::string out;
    out += "================ HTML ================\r\n" + src.html + "\r\n\r\n";
    out += "================ CSS ================\r\n" + (src.css.empty() ? std::string("(none)") : src.css) + "\r\n\r\n";
    out += "============= JavaScript =============\r\n" + (src.javascript.empty() ? std::string("(none)") : src.javascript) + "\r\n\r\n";
    out += "============= TypeScript =============\r\n" + (src.typescript.empty() ? std::string("(none)") : src.typescript) + "\r\n";
    return out;
}
static HWND hwndText;
static HWND hwndLinks;

static std::vector<std::string> history;
static int history_index = -1;
static std::vector<std::pair<std::string, std::string>> last_links;

void load_url_into_ui(const std::string& url, bool push_history = true) {
    try {
        HttpResponse response = http_get(url);
        SourceBundle src = extract_source_bundle(response.body);

        SetWindowTextA(hwndAddress, url.c_str());
        const std::string source_text = build_source_text(src);
        SetWindowTextA(hwndSource, source_text.c_str());
        const std::string css = extract_style_blocks(response.body);
        browser::RenderContext ctx = browser::parse_document(response.body, css);
        (void)ctx;

        std::string plain;
        extract_text_and_links(response.body, plain, last_links);

        SetWindowTextA(hwndText, plain.c_str());
        SetWindowTextA(hwndAddress, url.c_str());

        SendMessage(hwndLinks, LB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < last_links.size(); ++i) {
            std::ostringstream item;
            item << (i + 1) << ": " << last_links[i].first << " -> " << last_links[i].second;
            SendMessageA(hwndLinks, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.str().c_str()));
        }

        if (push_history) {
            if (history_index + 1 < static_cast<int>(history.size())) history.resize(history_index + 1);
            if (history.empty() || history.back() != url) {
                history.push_back(url);
                history_index = static_cast<int>(history.size()) - 1;
            }
        }
    } catch (const std::exception& ex) {
        SetWindowTextA(hwndSource, ex.what());
        SetWindowTextA(hwndText, ex.what());
    }
}

std::string get_address_text() {
    int len = GetWindowTextLengthW(hwndAddress);
    std::wstring w(len + 1, L'\0');
    GetWindowTextW(hwndAddress, &w[0], len + 1);
    return std::string(w.begin(), w.begin() + len);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"Address", WS_CHILD | WS_VISIBLE, 10, 14, 50, 20, hWnd, nullptr, nullptr, nullptr);
            hwndAddress = CreateWindowW(L"EDIT", L"https://duckduckgo.com", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                                        70, 10, 560, 24, hWnd, nullptr, nullptr, nullptr);

            CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          640, 10, 60, 24, hWnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"◀", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          710, 10, 36, 24, hWnd, reinterpret_cast<HMENU>(1002), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"▶", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          750, 10, 36, 24, hWnd, reinterpret_cast<HMENU>(1003), nullptr, nullptr);

            hwndSource = CreateWindowW(L"EDIT", nullptr,
                                       WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                                       10, 50, 960, 500, hWnd, nullptr, nullptr, nullptr);
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"Address", WS_CHILD | WS_VISIBLE, 10, 14, 50, 20, hWnd, nullptr, nullptr, nullptr);
            hwndAddress = CreateWindowW(L"EDIT", L"http://example.com",
                                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                                        70, 10, 430, 24, hWnd, nullptr, nullptr, nullptr);

            CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          510, 10, 60, 24, hWnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"◀", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          580, 10, 36, 24, hWnd, reinterpret_cast<HMENU>(1002), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"▶", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          620, 10, 36, 24, hWnd, reinterpret_cast<HMENU>(1003), nullptr, nullptr);

            hwndText = CreateWindowW(L"EDIT", nullptr,
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL |
                                         ES_READONLY | WS_VSCROLL,
                                     10, 50, 640, 390, hWnd, nullptr, nullptr, nullptr);

            hwndLinks = CreateWindowW(L"LISTBOX", nullptr,
                                      WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
                                      660, 50, 280, 390, hWnd, nullptr, nullptr, nullptr);
            break;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == 1001) {
                std::string url = get_address_text();
                if (url.find("://") == std::string::npos) url = "https://" + url;
                if (url.find("://") == std::string::npos) url = "http://" + url;
                load_url_into_ui(url, true);
            } else if (id == 1002) {
                if (history_index > 0) {
                    --history_index;
                    load_url_into_ui(history[history_index], false);
                }
            } else if (id == 1003) {
                if (history_index + 1 < static_cast<int>(history.size())) {
                    ++history_index;
                    load_url_into_ui(history[history_index], false);
                }
            } else if (HIWORD(wParam) == LBN_DBLCLK && reinterpret_cast<HWND>(lParam) == hwndLinks) {
                const int sel = static_cast<int>(SendMessage(hwndLinks, LB_GETCURSEL, 0, 0));
                if (sel != LB_ERR && sel < static_cast<int>(last_links.size())) {
                    std::string next = resolve_url(get_address_text(), last_links[sel].second);
                    if (!next.empty()) load_url_into_ui(next, true);
                    else SetWindowTextA(hwndText, "Blocked unsafe or malformed link target.");
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    constexpr wchar_t CLASS_NAME[] = L"ZephyrBrowserClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Zephyr Source Browser",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1000, 620,

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Zephyr Mini Browser",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 980, 520,
                                nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    load_url_into_ui("https://duckduckgo.com", true);
    load_url_into_ui("http://example.com", true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
