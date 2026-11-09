# Changelog

All notable changes to the NEXUS desktop browser are documented in this file.

## [0.2.0] - 2026-11-09 - Milestones 2 through 8 Production Release

### Added
- **Structured SQLite 3.47.2 Engine (`nexus_sqlite`):** Integrated embedded SQLite engine operating in Write-Ahead Logging (`WAL`) mode with persistent tables for history, bookmarks, downloads, media items, WireGuard VPN configs, site permissions, filter rules, and settings.
- **Privacy & Ad-Blocking Engine (`FilterEngine`):** Native C++ network request filter blocking 40+ major tracking and advertising domains (Google Analytics, DoubleClick, Facebook Events, Taboola, Outbrain, etc.) and telemetry beacons.
- **WireGuard VPN Manager (`VpnManager`):** Native WireGuard configuration parser, tunnel state machine, connection metrics tracking (uptime, sent/received bytes, interface IP), network kill-switch enforcement, and DNS leak protection.
- **Streaming Download Manager:** Non-buffering direct-to-disk streaming download handler with real-time transfer speed calculation, percent progress, filename sanitization, and UI drawer with Open File, Show in Folder, and Cancel actions.
- **Media Intelligence Sniffer:** Automatic network sniffer detecting streaming audio/video formats (HLS `.m3u8`, MPEG-DASH `.mpd`, MP4, WebM, MP3) with 1-click download queueing and URL copying.
- **Omnibox Autocomplete & Bookmarking:** Real-time query suggestion dropdown querying local history without network keystroke transmission, plus one-click star bookmarking and persistent Bookmarks Bar.
- **Advanced Navigation & Tab Stack:** Tab duplication, closed tab restoration stack (`Ctrl+Shift+T`), and in-page find bar (`Ctrl+F`) with forward/backward matching.
- **Zoom Engine:** Native zoom scaling (`Ctrl++`, `Ctrl+-`, `Ctrl+0`) with interactive toolbar badge.
- **Comprehensive UI Drawers:** Dedicated sliding panels for History, Bookmarks, Downloads, Media Sniffer, WireGuard VPN, and Browser Settings.
- **Expanded Test Suite:** 100% passing native test suite (`nexus_core_tests`) covering URL policy, FilterEngine adblocking, WireGuard config parsing, and SQLite operations.

## [0.1.0] - 2026-11-09 - Milestone 1 Foundation

### Added
- **CEF 156 Multi-Process Architecture:** Integration of pinned CEF distribution `156.0.26+g8ea0c73+chromium-156.0.8078.88` on Windows x64.
- **Enforced Sandbox Broker:** Application bootstrap stub `nexus.exe` initializes the Windows AppContainer/LPAC sandbox; refuses startup if `--no-sandbox` is provided or if the broker fails.
- **Custom Local Scheme (`nexus://`):** Registered standard isolated scheme serving `nexus://app/` (UI shell) and `nexus://newtab/` from an in-memory `AssetStore` with strict Content Security Policies.
- **React + TypeScript UI Shell:** Persistent desktop shell with `TabStrip`, `Toolbar` (Omnibox, Navigation controls, Security indicators), and `PrivacyDashboard`.
- **Strict Typed IPC Bridge:** JSON-RPC wire protocol (`@nexus/ipc-contracts`) with origin validation, method allowlists, and rate limiting; web content has zero access to native IPC.
- **CEF Native Views Integration:** `BrowserWindow` implementing `CefWindowDelegate` and `CefBrowserViewDelegate` with Alloy runtime style.
- **Content Security & Privacy Filters:** `ContentClient` and `PrivacyService` enforcing deny-by-default for media, geolocation, popups, and unreviewed downloads.
- **Cookie Policy Engine:** Native `CefCookieAccessFilter` providing Strict mode (blocks all cookies & strips referrers) and Balanced mode (blocks third-party tracking cookies & reduces cross-origin referrers).
- **CommandLine Omnibox Navigation & Automated Smoke Testing:** Added `--url=<url>` and `--smoke-test` flags for automated, headless end-to-end testing of real web navigation.
- **Unit and Integration Test Suites:** `nexus_core_tests` covering URL policies, input resolution, data directories, and launch options.
- **Full Architecture Documentation:** Published `README.md`, `ARCHITECTURE.md`, `PRIVACY.md`, `SECURITY.md`, `BUILDING.md`, `ROADMAP.md`, `PERFORMANCE.md`, and `CHANGELOG.md`.
