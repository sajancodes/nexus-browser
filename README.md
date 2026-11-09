# NEXUS

> Privacy-first, resource-efficient, cross-platform desktop browser.

NEXUS is engineered around a core principle: **the user owns their data**. It stores browsing history, bookmarks, downloads, cookies, and settings locally on the user's workstation by default. It transmits zero telemetry, blocks third-party cookies and tracking vectors, enforces strict sandboxing, and avoids unnecessary network communication.

---

## Architecture at a Glance

| Layer | Implementation |
|---|---|
| **Browser Engine** | Chromium Embedded Framework (CEF) 156 / Chromium 156 (`libcef`) |
| **Native Host** | C++20 with MSVC static runtime (`/MT`), multi-process architecture |
| **Sandbox** | Windows AppContainer / Low-Privilege AppContainer (LPAC) Broker |
| **UI Shell** | React 19 + TypeScript 5, Vite bundler, Dark aesthetic |
| **IPC Bridge** | Isolated JSON-RPC protocol over CEF V8 bindings (`nexus://app/`) |
| **Content Security** | Isolated Alloy-style content tabs with strict CSP and scheme validation |
| **Cookie Engine** | Native `CefCookieAccessFilter` with Strict & Balanced isolation modes |
| **Build System** | CMake 3.25+ and Ninja |

---

## Key Principles

1. **Zero Telemetry by Default:** No usage metrics, crash reports, behavioral pings, or device tracking.
2. **Local Data Ownership:** All state lives under `%LOCALAPPDATA%\NEXUS` (or custom `--user-data-dir`). No mandatory accounts or cloud dependencies.
3. **Defense-in-Depth Sandboxing:** The native broker refuses to launch without the CEF sandbox broker. Renderer, GPU, and network processes run in isolated, unprivileged containers.
4. **Origin & Privilege Isolation:** Web content runs in separate Alloy content views and has zero access to native OS facilities or the `nexus://app` IPC bridge.
5. **Cookie & Tracker Neutralization:** Strict mode blocks all cookies and wipes referrers; Balanced mode blocks third-party tracking cookies and limits referrers to cross-origin transitions.
6. **Deny-by-Default Web Permissions:** Camera, microphone, geolocation, popups, and intrusive web APIs are rejected without prompts.

---

## Directory Layout

```text
nexus/
├── apps/
│   └── desktop/
│       ├── frontend/          # React + TypeScript UI shell (TabStrip, Toolbar, Privacy Dashboard)
│       └── native/            # C++20 native application host & CEF integration
│           ├── core/          # Path management, URL policy, resolution logic
│           ├── src/           # Window delegates, scheme handlers, IPC router, privacy engine
│           └── tests/         # Native unit test suite (core_tests)
├── docs/                      # Technical architecture, security, and ADR documentation
├── resources/                 # Application assets, icons, themes
├── scripts/                   # Tooling, CEF dependency fetcher, PowerShell build scripts
├── shared/
│   └── ipc-contracts/         # TypeScript IPC contracts & validation schemas
├── tests/                     # Integration and end-to-end browser test suites
└── third_party/
    └── cef/                   # Pinned CEF binary distribution and CMake wrappers
```

---

## Quick Start

### Prerequisites

- **OS:** Windows 10/11 x64 (Linux & macOS support in roadmap)
- **Compiler:** Visual Studio 2022 Build Tools (with Desktop C++ Workload)
- **Build Tools:** CMake 3.25+, Ninja 1.11+
- **Frontend:** Node.js 20+, pnpm (`corepack enable`)
- **Python:** Python 3.9+ (for CEF fetch & verification)

### Building and Running

1. **Fetch CEF Binary Distribution:**
   ```powershell
   python scripts/fetch_cef.py
   ```
2. **Compile Frontend & Native Host:**
   ```powershell
   .\scripts\build.ps1
   ```
3. **Run Unit Tests:**
   ```powershell
   ctest --test-dir build\windows-x64-release -C Release --output-on-failure
   ```
4. **Launch NEXUS:**
   ```powershell
   .\build\windows-x64-release\bin\nexus.exe
   ```

To navigate to a specific URL or run automated smoke verification:
```powershell
.\build\windows-x64-release\bin\nexus.exe https://example.com
.\build\windows-x64-release\bin\nexus.exe --smoke-test --url=https://example.com
```

---

## Documentation

- [Architecture Guide](file:///c:/Users/ab429/Desktop/nexus/ARCHITECTURE.md)
- [Privacy Specification](file:///c:/Users/ab429/Desktop/nexus/PRIVACY.md)
- [Security Specification](file:///c:/Users/ab429/Desktop/nexus/SECURITY.md)
- [Building & Toolchain](file:///c:/Users/ab429/Desktop/nexus/BUILDING.md)
- [Milestone Roadmap](file:///c:/Users/ab429/Desktop/nexus/ROADMAP.md)
- [Performance & Benchmarks](file:///c:/Users/ab429/Desktop/nexus/PERFORMANCE.md)
