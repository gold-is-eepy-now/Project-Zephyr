# Zephyr Rust Browser Roadmap

## Mission
Build a fully open-source, Chromium-class browser without Google dependencies.

## Phase 0 (current)
- Rust networking and URL safety primitives
- Rust extraction + text rendering
- CLI shell and test harness

## Phase 1
- Integrate Servo html5ever/cssparser for standards parsing
- Build style tree and block/inline layout pass
- Introduce proper DOM model with incremental mutation

## Phase 2
- JS engine integration (Boa first, evaluate SpiderMonkey embedding)
- Event loop, timers, fetch/XHR, DOM APIs
- Browser process + renderer process split

## Phase 3
- GPU rendering pipeline (wgpu)
- Compositor, layers, scrolling/animation pipeline
- Native GUI shell (tao/winit + custom widgets)

## Phase 4
- Security hardening: sandboxing, site isolation, permissions
- Extensions framework and devtools protocol
- Web Platform Tests CI matrix

## Non-goals for early milestones
- No proprietary telemetry
- No proprietary sync service lock-in
