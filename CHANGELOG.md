# Changelog

All notable changes to the NEXUS desktop browser are documented in this file.

## [0.1.0] - 2026-11-09 - Milestone 1 Foundation

### Added
- **CEF 156 Multi-Process Architecture:** Integration of pinned CEF distribution `156.0.26+g8ea0c73+chromium-156.0.8078.88` on Windows x64.
- **Enforced Sandbox Broker:** Application bootstrap stub `nexus.exe` initializes the Windows AppContainer/LPAC sandbox; refuses startup if `--no-sandbox` is provided or if the broker fails.
- **Custom Local Scheme (`nexus://`):** Registered standard isolated scheme serving `nexus://app/` (UI shell) and `nexus://newtab/` from an in-memory `AssetStore` with strict Content Security Policies.
- **React + TypeScript UI Shell:** Persistent desktop shell with `TabStrip`, `Toolbar` (Omnibox, Navigation controls, Security indicators), and `PrivacyDashboard`.
- **Strict Typed IPC Bridge:** JSON-RPC wire protocol (`@nexus/ipc-contracts`) with origin validation, method allowlists, and rate limiting; web content has zero access to native IPC.
- **CEF Native Views Integration:** `BrowserWindow` implementing `CefWindowDelegate` and `CefBrowserViewDelegate` with Alloy runtime style.
- **Content Security & Privacy Filters:** `ContentClient` and `PrivacyService` enforcing deny-by-default for camera, microphone, geolocation, popups, and unreviewed downloads.
- **Cookie Policy Engine:** Native `CefCookieAccessFilter` providing Strict mode (blocks all cookies & strips referrers) and Balanced mode (blocks third-party tracking cookies & reduces cross-origin referrers).
- **CommandLine Omnibox Navigation & Automated Smoke Testing:** Added `--url=<url>` and `--smoke-test` flags for automated, headless end-to-end testing of real web navigation.
- **Unit and Integration Test Suites:** `nexus_core_tests` covering URL policies, input resolution, data directories, and launch options.
- **Full Architecture Documentation:** Published `README.md`, `ARCHITECTURE.md`, `PRIVACY.md`, `SECURITY.md`, `BUILDING.md`, `ROADMAP.md`, `PERFORMANCE.md`, and `CHANGELOG.md`.
