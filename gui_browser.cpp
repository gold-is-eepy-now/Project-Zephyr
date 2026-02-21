#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>

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

void load_url_into_ui(const std::string& url, bool push_history = true) {
    try {
        HttpResponse response = http_get(url);
        SourceBundle src = extract_source_bundle(response.body);

        SetWindowTextA(hwndAddress, url.c_str());
        const std::string source_text = build_source_text(src);
        SetWindowTextA(hwndSource, source_text.c_str());

        if (push_history) {
            if (history_index + 1 < static_cast<int>(history.size())) history.resize(history_index + 1);
            if (history.empty() || history.back() != url) {
                history.push_back(url);
                history_index = static_cast<int>(history.size()) - 1;
            }
        }
    } catch (const std::exception& ex) {
        SetWindowTextA(hwndSource, ex.what());
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
            break;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == 1001) {
                std::string url = get_address_text();
                if (url.find("://") == std::string::npos) url = "https://" + url;
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
                                nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    load_url_into_ui("https://duckduckgo.com", true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
