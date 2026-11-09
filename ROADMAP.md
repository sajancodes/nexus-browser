# NEXUS Engineering Roadmap

## Milestone 1 — Foundation (Complete)
- [x] Inspect development environment and toolchains (MSVC x64, CMake, Ninja, Node 24, pnpm 12, Python 3.11).
- [x] Pinned CEF 156 binary distribution with SHA-1 integrity check.
- [x] Configure CMake + Ninja with MSVC `/MT` static runtime matching CEF.
- [x] Native C++ host process layout using CEF Windows bootstrap stub and LPAC sandbox.
- [x] React 19 + TypeScript frontend UI shell (`nexus://app/`) with Vite bundler.
- [x] Strict typed IPC bridge with origin guards and JSON-RPC contract (`@nexus/ipc-contracts`).
- [x] CEF native Views Framework integration (`CefWindow`, `CefBrowserView`).
- [x] Local custom scheme handler (`nexus://`) with strict CSP.
- [x] Real web navigation to external HTTPS endpoints (verified with `https://example.com`).
- [x] Deny-by-default permission engine for media, geolocation, popups, and downloads.
- [x] Local storage directory isolation (`%LOCALAPPDATA%\NEXUS`).
- [x] Privacy Dashboard with local real-time counters and cookie mode toggling.
- [x] Comprehensive documentation suite (README, ARCHITECTURE, PRIVACY, SECURITY, BUILDING, ROADMAP, PERFORMANCE, CHANGELOG).

## Milestone 2 — Core Browsing (Complete)
- [x] Omnibox enhancements: live auto-completion against local history, search engine selection (DuckDuckGo, Brave, Startpage, SearXNG).
- [x] Tab lifecycle improvements: tab duplication, closed tab restore stack (`closed_tabs_` / `Ctrl+Shift+T`).
- [x] Structured SQLite 3.47.2 data layer for persistent browsing history, bookmarks, site permissions, and settings with WAL mode.
- [x] One-click bookmarking in Omnibox with star icon toggle and Bookmarks Bar.
- [x] Keyboard shortcuts expanded (find-in-page `Ctrl+F`, zoom `Ctrl++`/`Ctrl+-`/`Ctrl+0`, history `Ctrl+H`, downloads `Ctrl+J`, bookmarks `Ctrl+B`).
- [x] In-page find controller with forward/backward match navigation and clean selection clearing.

## Milestone 3 — Privacy Foundation & Content Blocking (Complete)
- [x] Native C++ `FilterEngine` integration for network-level tracker and advertising block.
- [x] Built-in blocklists for 40+ tracking networks, analytics endpoints, and ad telemetry patterns.
- [x] Ephemeral first-party state isolation and zero third-party cookie persistence in Strict mode.
- [x] Anti-fingerprinting and referrer trimming at network boundaries (`OnBeforeResourceLoad`).
- [x] Origin-level site permissions manager with allow/deny/prompt states.

## Milestone 4 — Native Download Manager (Complete)
- [x] Streaming disk downloads avoiding memory buffering via CEF `CefDownloadHandler`.
- [x] Real-time download progress, received bytes, file size, and transfer speed calculations.
- [x] Safe filename sanitization preventing directory traversal attacks.
- [x] Download management UI drawer with Open File, Show in Folder, and Cancel actions.
- [x] Download history persistence in local SQLite database.

## Milestone 5 — Media Intelligence (Complete)
- [x] Resource response sniffing for video/audio streams and manifests (`application/vnd.apple.mpegurl`, `application/dash+xml`, `video/mp4`, `audio/mpeg`, etc.).
- [x] Native media item capture and storage in SQLite.
- [x] Media intelligence drawer with stream format badges, copy link, and direct download dispatch.

## Milestone 6 — WireGuard VPN Integration (Complete)
- [x] Native WireGuard config parser (`[Interface]` and `[Peer]` blocks, Address, DNS, Endpoint).
- [x] Connection state machine (Disconnected, Connecting, Connected, Disconnecting, Error).
- [x] Network kill-switch toggle preventing non-tunnel traffic leaks.
- [x] DNS leak protection forcing tunnel DNS resolution.
- [x] VPN control panel with profile import, live metrics (uptime, bytes sent/received, IP), and switcher.

## Milestone 7 — Advanced Browser Features (Complete)
- [x] Page zoom level management with discrete levels and reset indicator.
- [x] Bookmarks manager drawer with search, manual bookmark creation, and quick navigation.
- [x] Browsing history manager with search, date formatting, and clear all.
- [x] Settings panel with privacy mode switcher, search engine selector, and system details.

## Milestone 8 — Production Readiness & Packaging (Complete)
- [x] Automated unit and integration test suite (`nexus_core_tests`) verifying URL policy, filter engine, WireGuard parser, and SQLite operations.
- [x] Headless end-to-end smoke testing (`--smoke-test --url=<url>`) verifying clean initialization, Alloy view attachment, and graceful shutdown.
- [x] Release build verification (`nexus.exe` and `nexus.dll`) staged with full CEF sandbox enforcement and LPAC AppContainer permissions.
- [x] Clean git repository tracking and documentation.
