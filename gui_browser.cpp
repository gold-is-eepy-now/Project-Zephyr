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
constexpr int IDC_BACK = 1001;
constexpr int IDC_FORWARD = 1002;
constexpr int IDC_RELOAD = 1003;
constexpr int IDC_HOME = 1004;
constexpr int IDC_GO = 1005;

HWND g_tab = nullptr;
HWND g_address = nullptr;
HWND g_page = nullptr;
HWND g_status = nullptr;

std::vector<std::string> g_history;
int g_history_index = -1;

std::string normalize(std::string u) {
    if (u.find("://") == std::string::npos) u = "https://" + u;
    return u;
}

void set_status(const std::string& s) {
    if (g_status) SendMessageA(g_status, SB_SETTEXTA, 0, reinterpret_cast<LPARAM>(s.c_str()));
}

std::string address_text() {
    int len = GetWindowTextLengthW(g_address);
    std::wstring w(len + 1, L'\0');
    GetWindowTextW(g_address, &w[0], len + 1);
    return std::string(w.begin(), w.begin() + len);
}

void update_nav(HWND hwnd) {
    EnableWindow(GetDlgItem(hwnd, IDC_BACK), g_history_index > 0);
    EnableWindow(GetDlgItem(hwnd, IDC_FORWARD), g_history_index + 1 < static_cast<int>(g_history.size()));
}

void layout(HWND hwnd) {
    RECT r{};
    GetClientRect(hwnd, &r);

    const int pad = 10;
    const int tab_h = 30;
    const int tb_h = 30;
    const int btn = 34;

    MoveWindow(g_tab, pad, pad, r.right - (2 * pad), tab_h, TRUE);

    int y = pad + tab_h + 4;
    int x = pad;

    MoveWindow(GetDlgItem(hwnd, IDC_BACK), x, y, btn, tb_h, TRUE); x += btn + 4;
    MoveWindow(GetDlgItem(hwnd, IDC_FORWARD), x, y, btn, tb_h, TRUE); x += btn + 4;
    MoveWindow(GetDlgItem(hwnd, IDC_RELOAD), x, y, btn, tb_h, TRUE); x += btn + 4;
    MoveWindow(GetDlgItem(hwnd, IDC_HOME), x, y, 56, tb_h, TRUE); x += 60;

    const int go_w = 56;
    const int addr_w = r.right - x - go_w - pad - 4;
    MoveWindow(g_address, x, y, addr_w, tb_h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_GO), x + addr_w + 4, y, go_w, tb_h, TRUE);

    SendMessageW(g_status, WM_SIZE, 0, 0);
    RECT sr{};
    GetWindowRect(g_status, &sr);
    const int status_h = sr.bottom - sr.top;

    int page_y = y + tb_h + 6;
    MoveWindow(g_page, pad, page_y, r.right - (2 * pad), r.bottom - page_y - status_h - pad, TRUE);
}

void load(HWND hwnd, const std::string& url, bool push_history) {
    try {
        set_status("Loading " + url + " ...");
        const HttpResponse resp = http_get(url);
        const std::string page = render_page_text(resp.body, 110);

        SetWindowTextA(g_address, url.c_str());
        SetWindowTextA(g_page, page.empty() ? "(No renderable content)" : page.c_str());

        if (push_history) {
            if (g_history_index + 1 < static_cast<int>(g_history.size())) g_history.resize(g_history_index + 1);
            if (g_history.empty() || g_history.back() != url) {
                g_history.push_back(url);
                g_history_index = static_cast<int>(g_history.size()) - 1;
            }
        }

        set_status("Done");
    } catch (const std::exception& ex) {
        SetWindowTextA(g_page, ex.what());
        set_status(std::string("Load error: ") + ex.what());
    }

    update_nav(hwnd);
}

}  // namespace

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitCommonControls();

            g_tab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE,
                                  0, 0, 100, 30, hwnd, nullptr, nullptr, nullptr);
            TCITEMW tab{};
            tab.mask = TCIF_TEXT;
            tab.pszText = const_cast<LPWSTR>(L"Zephyr");
            TabCtrl_InsertItem(g_tab, 0, &tab);

            CreateWindowW(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BACK), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L">", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_FORWARD), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"R", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RELOAD), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Home", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_HOME), nullptr, nullptr);

            g_address = CreateWindowW(L"EDIT", L"https://duckduckgo.com", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                      0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_GO), nullptr, nullptr);

            g_page = CreateWindowW(L"EDIT", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                                   0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            g_status = CreateWindowW(STATUSCLASSNAMEW, L"Ready", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                     0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            layout(hwnd);
            update_nav(hwnd);
            return 0;
        }

        case WM_SIZE:
            layout(hwnd);
            return 0;

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == IDC_GO) {
                load(hwnd, normalize(address_text()), true);
            } else if (id == IDC_BACK && g_history_index > 0) {
                --g_history_index;
                load(hwnd, g_history[g_history_index], false);
            } else if (id == IDC_FORWARD && g_history_index + 1 < static_cast<int>(g_history.size())) {
                ++g_history_index;
                load(hwnd, g_history[g_history_index], false);
            } else if (id == IDC_RELOAD) {
                load(hwnd, normalize(address_text()), false);
            } else if (id == IDC_HOME) {
                load(hwnd, "https://duckduckgo.com", true);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    constexpr wchar_t kClassName[] = L"ZephyrBrowserWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, kClassName, L"Zephyr Browser",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1200, 760,
                                nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    load(hwnd, "https://duckduckgo.com", true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
