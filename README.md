# Project Zephyr - Lightweight Web Page Browser

Zephyr now focuses on **readable page display** instead of showing raw fetch logs or source blobs.

## What this build does

- Fetches pages over HTTP/HTTPS (libcurl backend when available).
- Parses HTML into a lightweight DOM and applies basic CSS cascade rules.
- Renders readable page content to CLI and the Win32 GUI text viewport.
- Hides non-visual tags (`script`, `style`, `head`, `meta`, etc.) in the rendered view.
- Preserves link URLs inline so navigation targets remain visible.

## Important note

- This is still not a pixel-perfect browser engine. It is a text-first renderer.
- JavaScript/TypeScript are not executed.

## Build

### Linux / macOS

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/zephyr_cli http://frogfind.com
./build/zephyr_cli https://duckduckgo.com
```

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\zephyr_cli.exe http://frogfind.com
.\build\Release\zephyr_cli.exe https://duckduckgo.com
.\build\Release\zephyr_gui.exe
```

## GUI redesign roadmap

See `GUI_REDESIGN_PLAN.md`.
