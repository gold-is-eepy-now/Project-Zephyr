#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>

#include <string>
#include <vector>

#include "browser_core.h"

namespace {
constexpr int IDC_BACK = 1001;
constexpr int IDC_FORWARD = 1002;
constexpr int IDC_RELOAD = 1003;
constexpr int IDC_HOME = 1004;
constexpr int IDC_GO = 1005;
constexpr int IDC_TAB_1 = 1101;
constexpr int IDC_TAB_2 = 1102;
constexpr int IDC_TAB_3 = 1103;

HWND g_address = nullptr;
HWND g_page = nullptr;
HWND g_hero_title = nullptr;
HWND g_hero_subtitle = nullptr;
HWND g_card_1 = nullptr;
HWND g_card_2 = nullptr;
HWND g_card_3 = nullptr;

HFONT g_font_ui = nullptr;
HFONT g_font_title = nullptr;
HFONT g_font_tabs = nullptr;

HBRUSH g_brush_bg = nullptr;
HBRUSH g_brush_top = nullptr;
HBRUSH g_brush_hero = nullptr;
HBRUSH g_brush_card = nullptr;

COLORREF kBg = RGB(10, 14, 24);
COLORREF kTop = RGB(24, 30, 48);
COLORREF kHero = RGB(27, 43, 79);
COLORREF kCard = RGB(34, 49, 84);
COLORREF kText = RGB(235, 239, 248);
COLORREF kMuted = RGB(176, 188, 211);

std::vector<std::string> g_history;
int g_history_index = -1;

std::string normalize(std::string u) {
    if (u.find("://") == std::string::npos) u = "https://" + u;
    return u;
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

    const int pad = 14;
    const int tab_h = 34;
    const int nav_h = 34;

    int y = pad;
    MoveWindow(GetDlgItem(hwnd, IDC_TAB_1), pad, y, 220, tab_h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_TAB_2), pad + 226, y, 170, tab_h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_TAB_3), pad + 402, y, 170, tab_h, TRUE);

    y += tab_h + 10;
    int x = pad;
    MoveWindow(GetDlgItem(hwnd, IDC_BACK), x, y, 34, nav_h, TRUE); x += 38;
    MoveWindow(GetDlgItem(hwnd, IDC_FORWARD), x, y, 34, nav_h, TRUE); x += 38;
    MoveWindow(GetDlgItem(hwnd, IDC_RELOAD), x, y, 34, nav_h, TRUE); x += 42;

    const int go_w = 64;
    const int home_w = 74;
    const int address_w = r.right - x - go_w - home_w - (pad * 2) - 8;
    MoveWindow(g_address, x, y, address_w, nav_h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_GO), x + address_w + 4, y, go_w, nav_h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_HOME), x + address_w + go_w + 8, y, home_w, nav_h, TRUE);

    const int hero_y = y + nav_h + 12;
    const int hero_h = 220;
    MoveWindow(g_hero_title, pad + 24, hero_y + 40, 620, 54, TRUE);
    MoveWindow(g_hero_subtitle, pad + 24, hero_y + 100, 620, 44, TRUE);

    const int cards_y = hero_y + hero_h - 88;
    const int card_w = (r.right - (pad * 2) - 24) / 3;
    MoveWindow(g_card_1, pad + 6, cards_y, card_w, 82, TRUE);
    MoveWindow(g_card_2, pad + 12 + card_w, cards_y, card_w, 82, TRUE);
    MoveWindow(g_card_3, pad + 18 + (card_w * 2), cards_y, card_w, 82, TRUE);

    const int page_y = hero_y + hero_h + 12;
    MoveWindow(g_page, pad, page_y, r.right - (pad * 2), r.bottom - page_y - pad, TRUE);
}

void show_loaded_content(const std::string& url, const std::string& page) {
    SetWindowTextA(g_address, url.c_str());
    SetWindowTextA(g_page, page.empty() ? "(No renderable content)" : page.c_str());

    SetWindowTextW(g_hero_title, L"EXPLORE THE OPEN WEB");
    SetWindowTextW(g_hero_subtitle, L"Privacy-first, open-source browsing with a modern desktop feel.");
}

void load(HWND hwnd, const std::string& url, bool push_history) {
    try {
        const HttpResponse resp = http_get(url);
        const std::string page = render_page_text(resp.body, 110);
        show_loaded_content(url, page);

        if (push_history) {
            if (g_history_index + 1 < static_cast<int>(g_history.size())) g_history.resize(g_history_index + 1);
            if (g_history.empty() || g_history.back() != url) {
                g_history.push_back(url);
                g_history_index = static_cast<int>(g_history.size()) - 1;
            }
        }
    } catch (const std::exception& ex) {
        SetWindowTextA(g_page, ex.what());
    }

    update_nav(hwnd);
}

}  // namespace

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_brush_bg = CreateSolidBrush(kBg);
            g_brush_top = CreateSolidBrush(kTop);
            g_brush_hero = CreateSolidBrush(kHero);
            g_brush_card = CreateSolidBrush(kCard);

            g_font_ui = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_title = CreateFontW(-48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, L"Georgia");
            g_font_tabs = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            CreateWindowW(L"BUTTON", L"  🌐  Explore Destinations   ✕", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_TAB_1), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"  ✈  Book Flights", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_TAB_2), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"  ⚙  Settings", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_TAB_3), nullptr, nullptr);

            CreateWindowW(L"BUTTON", L"←", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BACK), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"→", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_FORWARD), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"↻", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RELOAD), nullptr, nullptr);

            g_address = CreateWindowW(L"EDIT", L"https://duckduckgo.com",
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                      0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_GO), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Home", WS_CHILD | WS_VISIBLE | BS_FLAT,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_HOME), nullptr, nullptr);

            g_hero_title = CreateWindowW(L"STATIC", L"EXPLORE THE OPEN WEB", WS_CHILD | WS_VISIBLE,
                                         0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            g_hero_subtitle = CreateWindowW(L"STATIC", L"Privacy-first, open-source browsing with a modern desktop feel.", WS_CHILD | WS_VISIBLE,
                                            0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            g_card_1 = CreateWindowW(L"STATIC", L"1. Open Source Core\nLearn More", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            g_card_2 = CreateWindowW(L"STATIC", L"2. Secure by Default\nLearn More", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            g_card_3 = CreateWindowW(L"STATIC", L"3. Rust Engine\nLearn More", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            g_page = CreateWindowW(L"EDIT", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                                   0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

            SendMessageW(g_address, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_ui), TRUE);
            SendMessageW(g_page, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_ui), TRUE);
            SendMessageW(g_hero_title, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_title), TRUE);
            SendMessageW(g_hero_subtitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_ui), TRUE);
            SendMessageW(g_card_1, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_ui), TRUE);
            SendMessageW(g_card_2, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_ui), TRUE);
            SendMessageW(g_card_3, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_ui), TRUE);
            SendMessageW(GetDlgItem(hwnd, IDC_TAB_1), WM_SETFONT, reinterpret_cast<WPARAM>(g_font_tabs), TRUE);
            SendMessageW(GetDlgItem(hwnd, IDC_TAB_2), WM_SETFONT, reinterpret_cast<WPARAM>(g_font_tabs), TRUE);
            SendMessageW(GetDlgItem(hwnd, IDC_TAB_3), WM_SETFONT, reinterpret_cast<WPARAM>(g_font_tabs), TRUE);

            layout(hwnd);
            update_nav(hwnd);
            return 0;
        }

        case WM_SIZE:
            layout(hwnd);
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND ctl = reinterpret_cast<HWND>(lParam);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, (ctl == g_hero_subtitle) ? kMuted : kText);

            if (ctl == g_hero_title || ctl == g_hero_subtitle) return reinterpret_cast<LRESULT>(g_brush_hero);
            if (ctl == g_card_1 || ctl == g_card_2 || ctl == g_card_3) return reinterpret_cast<LRESULT>(g_brush_card);
            return reinterpret_cast<LRESULT>(g_brush_bg);
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, kText);
            SetBkColor(hdc, RGB(20, 28, 45));
            return reinterpret_cast<LRESULT>(g_brush_top);
        }

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

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT r{};
            GetClientRect(hwnd, &r);
            FillRect(hdc, &r, g_brush_bg);

            RECT top = r;
            top.bottom = 118;
            FillRect(hdc, &top, g_brush_top);

            RECT hero = r;
            hero.left += 14;
            hero.right -= 14;
            hero.top = 102;
            hero.bottom = 322;
            FillRect(hdc, &hero, g_brush_hero);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            if (g_font_ui) DeleteObject(g_font_ui);
            if (g_font_title) DeleteObject(g_font_title);
            if (g_font_tabs) DeleteObject(g_font_tabs);
            if (g_brush_bg) DeleteObject(g_brush_bg);
            if (g_brush_top) DeleteObject(g_brush_top);
            if (g_brush_hero) DeleteObject(g_brush_hero);
            if (g_brush_card) DeleteObject(g_brush_card);
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

    HWND hwnd = CreateWindowExW(0, kClassName, L"Zephyr Browser - Glass UI",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1240, 820,
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
