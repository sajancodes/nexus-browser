# NEXUS Engineering Roadmap

## Milestone 1 — Foundation (Current / Complete)
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

## Milestone 2 — Core Browsing
- [ ] Omnibox enhancements: auto-completion against local history, search engine selection.
- [ ] Tab lifecycle improvements: tab duplication, close other tabs, tab drag reordering.
- [ ] Structured SQLite data layer for persistent browsing history and bookmarks.
- [ ] Context menus (native/Alloy integration).
- [ ] Keyboard shortcuts expanded (find-in-page `Ctrl+F`, zoom `Ctrl++`/`Ctrl+-`, history `Ctrl+H`).
- [ ] In-page find controller.

## Milestone 3 — Privacy Foundation & Content Blocking
- [ ] Brave `adblock-rust` engine integration for network-level request blocking.
- [ ] EasyList, EasyPrivacy, and uBlock filter list parsing and local rule caching.
- [ ] Origin storage partitioning and ephemeral first-party state expiration.
- [ ] Anti-fingerprinting protections: Canvas/WebGL noise generation and AudioContext normalization.
- [ ] Configurable DNS-over-HTTPS (DoH) resolver selection (Cloudflare, Quad9, Mullvad).
- [ ] Dedicated private browsing window mode with in-memory request contexts.

## Milestone 4 — Native Download Manager
- [ ] Resumable streaming downloads directly to disk (avoiding memory buffering).
- [ ] Download progress, transfer rate, and time remaining calculations.
- [ ] Safe filename sanitization and file duplicate handling.
- [ ] Download history tracking in SQLite.

## Milestone 5 — Media Intelligence
- [ ] DOM and network media sniffer (HTML5 video/audio/images, HLS/DASH manifest detection).
- [ ] Native FFmpeg integration for container muxing and format conversion.
- [ ] Selective yt-dlp integration for supported open media streams.

## Milestone 6 — WireGuard VPN Integration
- [ ] Native WireGuard integration using WinTun driver on Windows.
- [ ] WireGuard `.conf` import and cryptographic key protection via DPAPI.
- [ ] Connection state management and automated kill-switch route enforcement.
- [ ] WebRTC and DNS leak verification tests.

## Milestone 7 — Advanced Browser Features
- [ ] Multi-profile management (Work, Personal, Ephemeral).
- [ ] Session restore and crash recovery.
- [ ] Per-site permission exception management.

## Milestone 8 — Production Readiness & Packaging
- [ ] Full security audit and fuzzing.
- [ ] Memory and CPU profiling across 1, 5, 10, 50 tab benchmarks.
- [ ] Windows MSI / WiX installer generation and EV code signing workflow.
- [ ] Linux AppImage and macOS universal binary packaging.
