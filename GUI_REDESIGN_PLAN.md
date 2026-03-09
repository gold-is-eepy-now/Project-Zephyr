# GUI Redesign Plan (Chrome-like productivity, Zephyr identity)

## Design goals
- Familiar tab-centric workflow (address/search omnibox, tabs, profile menu, settings) without cloning Chromium visuals.
- Distinct Zephyr visual language: high-contrast adaptive theme, vertical utility rail, command palette, split-view reading mode.
- Performance-first architecture that can evolve into multiprocess rendering later.

## Proposed information architecture
1. **Top frame**
   - Tab strip (pinned + normal + tab groups)
   - Omnibox (URL + search + permissions/status chips)
   - Action cluster (back/forward/reload, bookmark, share, profile)
2. **Left utility rail**
   - History, downloads, bookmarks, workspaces, dev-tools launcher
3. **Main content area**
   - Web viewport
   - Optional side-pane (reader mode, notes, AI summary, inspector)
4. **Bottom status lane**
   - Network status, TLS/security badge details, process/memory indicators

## UX feature roadmap
### Phase 1 (foundation)
- New layout system + responsive resizing behaviors
- Tab model/state store and command routing
- Omnibox parser (URL/search/autocomplete)
- Unified navigation history model

### Phase 2 (interaction)
- Context menus + keyboard shortcuts + command palette
- Download manager and permission prompts
- Reader mode and split-view

### Phase 3 (advanced)
- Workspace/session restore
- Tab grouping and sidebar workflows
- Built-in diagnostics panel (network/timing/security)

## Technical architecture recommendations
- Introduce a platform-agnostic UI abstraction layer:
  - `ui_shell` (window/app lifecycle)
  - `ui_components` (tab strip, omnibox, panels)
  - `browser_state` (tabs/history/permissions)
  - `render_bridge` (web content embedding)
- Move to event-driven async model for network + parsing tasks.
- Prepare for process isolation boundaries:
  - browser process (UI/state)
  - renderer process abstraction (future)

## Security UX model
- First-class security indicators in omnibox:
  - HTTPS lock + certificate metadata
  - mixed-content and unsafe-navigation warnings
- Permission prompts with per-origin persistence (camera/mic/notifications/downloads).
- Safe browsing interstitial design for blocked destinations.

## Success metrics
- Startup < 300ms shell render on dev hardware
- Navigation action feedback < 100ms
- Tab switch perceived latency < 50ms
- Accessibility baseline: keyboard-only and screen-reader-friendly navigation
