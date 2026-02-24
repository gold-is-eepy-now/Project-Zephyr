# Project Zephyr (Full Rewrite)

This revision is a **full from-scratch rewrite** of Zephyr’s implementation while preserving the same repository shape and build targets.

## What Zephyr is now

- A lightweight HTTP/HTTPS text browser core.
- A DOM + CSS based text renderer for readable page output.
- A CLI browser frontend.
- A Chromium-inspired Win32 shell (tab strip, nav toolbar, omnibox, status bar).

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

## Notes

- JavaScript/TypeScript are extracted but not executed.
- The renderer is text-first, not a pixel browser engine.
