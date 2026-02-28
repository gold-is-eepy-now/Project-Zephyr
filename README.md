# Zephyr Browser (Rust Rewrite)

Zephyr is now a **Rust-first browser project** with a new architecture intended to scale toward a Chromium-class fully open-source browser stack.

## Current state

This repository now includes a Rust implementation for:

- HTTP/HTTPS fetching (Reqwest + native TLS)
- URL parsing and safe navigation filtering
- HTML/CSS/JS/TS source extraction
- Text-oriented page rendering
- Interactive CLI browser with history/navigation
- A cinematic dark "glass" Win32 GUI shell inspired by modern browser chrome (tabs, omnibox, hero panel, cards)


## Engine hardening (started)

Zephyr now includes first-pass engine hardening primitives in Rust:

- Spec-oriented HTML parsing with `html5ever` into an internal document model.
- A fuller CSS parser/cascade starter using `cssparser` (selectors, declarations, specificity/source-order matching).
- Early layout tree generation from parsed DOM/CSS (`compute_layout`) as groundwork for real visual layout.
- JavaScript runtime integration starter using `boa_engine` (`execute_javascript`).

## Vision: Chromium-class open browser

To be comparable to Chromium, Zephyr needs phased growth:

1. **Engine hardening**
   - Spec-compliant HTML parser
   - Full CSS parser/cascade/layout
   - JavaScript runtime integration
2. **Rendering stack**
   - Display list + GPU compositor (wgpu/vulkan/metal/dx)
   - Text shaping, fonts, accessibility tree
3. **Platform/browser features**
   - Multi-process sandbox model
   - Tabs/process isolation
   - Extension APIs
   - DevTools protocol and inspector UI
4. **Security + web compatibility**
   - CSP, CORS, mixed-content, origin model
   - WPT-based compatibility tracking

## Build and run (Rust)

```bash
cargo build
cargo test
cargo run -- https://duckduckgo.com
cargo run -- http://frogfind.com
```

## Notes

- This is an early Rust foundation, **not yet a full graphical browser engine**.
- The existing C++ artifacts remain in the repo history, but active direction is Rust.
