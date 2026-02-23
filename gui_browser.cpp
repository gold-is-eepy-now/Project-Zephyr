#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

#include "browser_core.h"

#pragma comment(lib, "Comctl32.lib")

namespace {
constexpr int IDC_GO = 1001;
constexpr int IDC_BACK = 1002;
constexpr int IDC_FORWARD = 1003;
constexpr int IDC_RELOAD = 1004;
constexpr int IDC_HOME = 1005;
constexpr int IDC_TAB = 1006;

constexpr int kPadding = 10;
constexpr int kTabHeight = 30;
constexpr int kToolbarHeight = 34;

HWND hwndTab;
HWND hwndAddress;
HWND hwndPage;
HWND hwndStatus;

std::vector<std::string> history;
int history_index = -1;

void set_status(const std::string& text) {
    if (hwndStatus) SendMessageA(hwndStatus, SB_SETTEXTA, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

std::string normalize_url(std::string url) {
    if (url.find("://") == std::string::npos) url = "https://" + url;
    return url;
}

void apply_layout(HWND hWnd) {
    RECT rc {};
    GetClientRect(hWnd, &rc);

    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    const int tab_y = kPadding;
    MoveWindow(hwndTab, kPadding, tab_y, width - (2 * kPadding), kTabHeight, TRUE);

    const int toolbar_y = tab_y + kTabHeight + 2;
    const int button_w = 36;
    const int home_w = 56;
    const int go_w = 56;

    int x = kPadding;
    MoveWindow(GetDlgItem(hWnd, IDC_BACK), x, toolbar_y, button_w, kToolbarHeight - 6, TRUE);
    x += button_w + 6;
    MoveWindow(GetDlgItem(hWnd, IDC_FORWARD), x, toolbar_y, button_w, kToolbarHeight - 6, TRUE);
    x += button_w + 6;
    MoveWindow(GetDlgItem(hWnd, IDC_RELOAD), x, toolbar_y, button_w, kToolbarHeight - 6, TRUE);
    x += button_w + 6;
    MoveWindow(GetDlgItem(hWnd, IDC_HOME), x, toolbar_y, home_w, kToolbarHeight - 6, TRUE);
    x += home_w + 8;

    const int address_w = width - x - go_w - kPadding;
    MoveWindow(hwndAddress, x, toolbar_y, address_w, kToolbarHeight - 6, TRUE);
    MoveWindow(GetDlgItem(hWnd, IDC_GO), x + address_w + 6, toolbar_y, go_w, kToolbarHeight - 6, TRUE);

    RECT status_rc {};
    SendMessageW(hwndStatus, WM_SIZE, 0, 0);
    GetWindowRect(hwndStatus, &status_rc);
    const int status_h = status_rc.bottom - status_rc.top;

    const int page_y = toolbar_y + kToolbarHeight + 4;
    const int page_h = height - page_y - status_h - kPadding;
    MoveWindow(hwndPage, kPadding, page_y, width - (2 * kPadding), page_h, TRUE);
}

void update_nav_buttons(HWND hWnd) {
    EnableWindow(GetDlgItem(hWnd, IDC_BACK), history_index > 0 ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_FORWARD), history_index + 1 < static_cast<int>(history.size()) ? TRUE : FALSE);
}

void load_url_into_ui(HWND hWnd, const std::string& url, bool push_history = true) {
    try {
        set_status("Loading " + url + " ...");
        HttpResponse response = http_get(url);
        const std::string page_text = render_page_text(response.body, 110);

        SetWindowTextA(hwndAddress, url.c_str());
        SetWindowTextA(hwndPage, page_text.empty() ? "(No renderable body content)" : page_text.c_str());

        if (push_history) {
            if (history_index + 1 < static_cast<int>(history.size())) history.resize(history_index + 1);
            if (history.empty() || history.back() != url) {
                history.push_back(url);
                history_index = static_cast<int>(history.size()) - 1;
            }
        }

        set_status("Done");
        update_nav_buttons(hWnd);
    } catch (const std::exception& ex) {
        SetWindowTextA(hwndPage, ex.what());
        set_status(std::string("Load error: ") + ex.what());
        update_nav_buttons(hWnd);
    }
}

std::string get_address_text() {
    const int len = GetWindowTextLengthW(hwndAddress);
    std::wstring w(len + 1, L'\0');
    GetWindowTextW(hwndAddress, &w[0], len + 1);
    return std::string(w.begin(), w.begin() + len);
}
}  // namespace

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitCommonControls();

            hwndTab = CreateWindowW(WC_TABCONTROLW, L"",
                                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                    0, 0, 100, 100, hWnd,
                                    reinterpret_cast<HMENU>(IDC_TAB), nullptr, nullptr);
            TCITEMW tie {};
            tie.mask = TCIF_TEXT;
            tie.pszText = const_cast<LPWSTR>(L"Zephyr");
            TabCtrl_InsertItem(hwndTab, 0, &tie);

            CreateWindowW(L"BUTTON", L"◀", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(IDC_BACK), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"▶", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(IDC_FORWARD), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"⟳", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(IDC_RELOAD), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Home", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(IDC_HOME), nullptr, nullptr);

            hwndAddress = CreateWindowW(L"EDIT", L"https://duckduckgo.com",
                                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
                                        0, 0, 0, 0, hWnd, nullptr, nullptr, nullptr);

            CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(IDC_GO), nullptr, nullptr);

            hwndPage = CreateWindowW(L"EDIT", nullptr,
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
                                         WS_VSCROLL | WS_HSCROLL,
                                     0, 0, 0, 0, hWnd, nullptr, nullptr, nullptr);

            hwndStatus = CreateWindowW(STATUSCLASSNAMEW, L"Ready",
                                       WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                       0, 0, 0, 0, hWnd, nullptr, nullptr, nullptr);

            apply_layout(hWnd);
            update_nav_buttons(hWnd);
            break;
        }

        case WM_SIZE:
            apply_layout(hWnd);
            return 0;

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == IDC_GO) {
                load_url_into_ui(hWnd, normalize_url(get_address_text()), true);
            } else if (id == IDC_BACK) {
                if (history_index > 0) {
                    --history_index;
                    load_url_into_ui(hWnd, history[history_index], false);
                }
            } else if (id == IDC_FORWARD) {
                if (history_index + 1 < static_cast<int>(history.size())) {
                    ++history_index;
                    load_url_into_ui(hWnd, history[history_index], false);
                }
            } else if (id == IDC_RELOAD) {
                const std::string url = normalize_url(get_address_text());
                load_url_into_ui(hWnd, url, false);
            } else if (id == IDC_HOME) {
                load_url_into_ui(hWnd, "https://duckduckgo.com", true);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    constexpr wchar_t CLASS_NAME[] = L"ZephyrBrowserClass";

    WNDCLASSW wc {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Zephyr Browser",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1200, 760,
                                nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    load_url_into_ui(hwnd, "https://duckduckgo.com", true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
