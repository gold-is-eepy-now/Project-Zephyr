# Mini C++ Console Browser

This is a tiny, original educational browser written in C++.

Features
- Simple HTTP/1.0 client implemented with sockets (no TLS / HTTPS)
- Tiny HTML extractor that strips tags and finds simple <a href> links
- Console UI: displays text and numbered links, lets you follow links, go back/forward

Limitations / assumptions
- Only supports http:// URLs (no HTTPS). This keeps the implementation from relying on external TLS libraries.
- HTML parsing is extremely small and not standards-compliant. It's meant for demonstration only.

Build (Windows, using MinGW or similar with g++)

Open PowerShell in this folder and run:

```powershell
g++ -std=c++17 -O2 browser.cpp -o mini_browser.exe -lws2_32
```

On Unix (Linux/macOS), build with:

```bash
g++ -std=c++17 -O2 browser.cpp -o mini_browser
```

Usage

```powershell
# Run and enter a URL when prompted
.\mini_browser.exe

# Or provide a URL directly
.\mini_browser.exe http://example.com

GUI build (Win32)

To build the GUI version, run:

```powershell
g++ -std=c++17 -O2 gui_browser.cpp browser_core.cpp -o gui_browser.exe -lws2_32 -lgdi32
```

Run the GUI version:

```powershell
.\gui_browser.exe
```
```

Interactive commands
- Enter a number to follow the corresponding link shown in the page.
- url <some-url> — open a new URL (prefix with http:// if omitted)
- back, forward — navigate history
- quit — exit

Testing
- The repository includes a small PowerShell script (`run_test.ps1`) to build and fetch `http://example.com` and show output.

Next steps (suggested)
- Add HTTPS support with OpenSSL or platform TLS APIs.
- Improve HTML parsing and rendering (basic layout, CSS).
- Add a GUI (e.g., using a cross-platform toolkit) for richer rendering.
