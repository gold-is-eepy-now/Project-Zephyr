# Project Zephyr - HTML/CSS/JS/TS Source Browser

Zephyr is now oriented as a **source-view browser**:
when you open a website, the UI shows extracted source sections for:

- HTML
- CSS
- JavaScript
- TypeScript

## What this build does

- Fetches pages over HTTP/HTTPS (libcurl backend when available).
- Displays webpage source blocks instead of rendered page layout.
- Extracts inline `<style>` as CSS.
- Extracts inline/external `<script>` references as JavaScript.
- Detects `text/typescript` / `application/typescript` style script types as TypeScript.
- Keeps safe URL filtering for dangerous schemes (`javascript:`, `data:`, `file:`, `vbscript:`).

## Important note

- JavaScript/TypeScript are **extracted and displayed**, not executed.
- This is still not a full standards-compliant browser engine.

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
