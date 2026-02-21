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
# Project Zephyr - Modern HTTP/HTTPS-capable Browser Core 

Project Zephyr is a compact C++17 browser prototype focused on learning and iteration.
This revision adds **HTTPS-capable transport (via libcurl when available)**, stronger parsing behavior, and a dedicated GUI redesign plan.

## Current capabilities

- HTTP + HTTPS fetching (libcurl backend when available; socket fallback)
- Redirect handling and protocol restrictions to `http`/`https`
- Safe navigation filtering (`javascript:`, `data:`, `file:`, `vbscript:` blocked)
- HTML extraction that skips `script`/`style`, decodes entities, and extracts links
- DOM parser with attributes, comments, void tags, raw-text tag handling
- CSS parser with selector lists, simple descendant support, specificity/source-order cascade
- CLI browser frontend + Win32 GUI shell
- Core unit tests via CTest

## Build

### Linux / macOS

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/zephyr_cli http://frogfind.com
./build/zephyr_cli https://duckduckgo.com
./build/zephyr_cli https://example.com
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
.\build\Release\zephyr_cli.exe https://example.com
.\build\Release\zephyr_gui.exe
```

## Notes on standards and scope

- This project is still **not** a fully standards-compliant browser engine.
- It is not equivalent to Firefox/Chromium-class browsers in JS runtime hardening, process isolation, sandboxing, and full HTML/CSS rendering compliance.
- The goal is a safer, more modern educational architecture that can evolve incrementally.

## GUI redesign plan

See `GUI_REDESIGN_PLAN.md` for a full chrome-like (but distinctly Zephyr) redesign roadmap.
