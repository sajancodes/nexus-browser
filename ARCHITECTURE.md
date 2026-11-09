# NEXUS Architecture & System Design

## 1. System Overview

NEXUS is a privacy-first desktop browser engineered in C++20 and Chromium Embedded Framework (CEF), paired with an isolated, single-instance React/TypeScript user interface shell.

The core architectural tenet of NEXUS is **separation of concerns across strict isolation boundaries**:
- **Native Host (C++20):** Manages OS windowing, CEF message pump, process orchestration, storage directories, cookie interception, security policies, and IPC routing.
- **UI Shell (React + TypeScript):** Hosted at `nexus://app/` in a dedicated, trusted Chromium `BrowserView`. Renders tabs, navigation bar, address input, and privacy dashboard without direct OS or filesystem access.
- **Untrusted Web Content:** Rendered in standard, fully sandboxed CEF content `BrowserView` instances running Alloy runtime style. Web content has zero access to native APIs, local schemes, or internal IPC endpoints.

```
+-------------------------------------------------------------------------------+
|                             NEXUS Main Window                                 |
|                                                                               |
|  +-------------------------------------------------------------------------+  |
|  |  UI Shell View (nexus://app/) [kShellHeight = 88px]                     |  |
|  |  [TabStrip: + | New Tab | Active Tab x]  [Shield]  [URL / Omnibox]      |  |
|  +-------------------------------------------------------------------------+  |
|                                                                               |
|  +-------------------------------------------------------------------------+  |
|  |  Active Content Tab View (e.g. https://example.com)                     |  |
|  |  (Sandboxed Chromium Renderer, LPAC AppContainer, Zero Native IPC)     |  |
|  |                                                                         |  |
|  +-------------------------------------------------------------------------+  |
+-------------------------------------------------------------------------------+
```

---

## 2. Process & Sandboxing Architecture

Chromium's multi-process model is preserved without compromise. NEXUS explicitly rejects configurations that degrade security boundaries.

### 2.1 Process Hierarchy on Windows

1. **Bootstrap Host (`nexus.exe`):**
   - Built from CEF's official `bootstrap.exe` stub.
   - Responsible for launching the sandbox broker and verifying OS security tokens.
   - Loads `nexus.dll` dynamically and invokes `RunWinMain(..., sandbox_info)`.
   - **Hard Security Constraint:** If `sandbox_info` is null or `--no-sandbox` is detected, NEXUS terminates immediately with an error dialog. Running without sandbox protection is prohibited.

2. **Browser Process (`nexus.dll`):**
   - Coordinates window creation, tabs, custom schemes, and SQLite storage.
   - Runs the main CEF UI thread message pump (`CefRunMessageLoop`).
   - Owns the `CefCookieManager` and request interceptors.

3. **Renderer Processes:**
   - One process for the internal UI shell (`nexus://app/`).
   - Isolated site-specific processes for web content tabs (Site Isolation).
   - Renderers run inside Low-Privilege AppContainers (LPAC) with the SID `*S-1-15-2-2` granted read/execute access to staged binary directories only.

4. **GPU & Utility Processes:**
   - Dedicated child processes for Direct3D/Vulkan hardware acceleration and network service.

---

## 3. Native / Frontend IPC Architecture

NEXUS does not inject arbitrary Node.js bindings or raw OS commands into the web view.

### 3.1 Strict Security Invariants

- **Origin Verification:** The native V8 bridge (`window.__nexusNative.postMessage`) is injected *strictly and only* into frames where:
  1. `frame->IsMain() == true`
  2. `core::IsInternalAppUrl(frame->GetURL()) == true` (i.e. `nexus://app/*`)
  3. The browser identifier matches `shell_browser_ids_` tracked by `NexusRenderProcessHandler`.
- Untrusted web content (`https://*`, `http://*`, `nexus://newtab/*`) never receives the `__nexusNative` object.
- Any attempt by external web scripts to call IPC fails silently or throws an exception.

### 3.2 Wire Protocol

Communication uses a typed JSON-RPC contract (`@nexus/ipc-contracts`):

```json
{
  "v": 1,
  "id": "req-14",
  "method": "browser.navigate",
  "payload": {
    "url": "https://example.com"
  }
}
```

The native `IpcRouter` enforces:
1. Protocol version validation (`v == 1`).
2. Method allowlist matching (`app.getInfo`, `browser.getState`, `browser.createTab`, `browser.navigate`, `privacy.setCookieMode`, etc.).
3. Request rate limiting (rejects bursts over threshold).
4. Maximum payload size bounds (64 KB request, 256 KB response).
5. Schema and URL validation (blocks internal schemes or dangerous protocol handlers).

---

## 4. Window and View Layout Strategy

Instead of embedding Chromium views inside Electron or wrapping an OS WebView inside an external framework:
- NEXUS utilizes CEF's native Views Framework (`CefWindow`, `CefBrowserView`, `CefWindowDelegate`).
- `BrowserWindow` hosts two view categories inside a single top-level native window:
  - **`shell_view_`:** Rendered at `(0, 0, width, kShellHeight)` (height: 88px) during standard browsing. When the user opens the Privacy Dashboard, `shell_view_` expands to `(0, 0, width, height)` to present full-screen dashboard statistics.
  - **Tab Views (`tab.view`):** The active tab's view occupies `(0, kShellHeight, width, height - kShellHeight)`. Inactive tabs remain hidden (`SetVisible(false)`).
- When tabs are closed, the corresponding `CefBrowser` is destroyed and resources are immediately reclaimed.

---

## 5. Architectural Trade-offs & Decisions

| Decision | Alternative Considered | Justification |
|---|---|---|
| **CEF Views Framework** | Custom Win32 / HWND child windows | CEF Views provides native DPI scaling, cross-platform window management, unified accessibility, and clean view layering without complex Win32 subclassing. |
| **Static Runtime (`/MT`)** | Dynamic CRT (`/MD`) | CEF binary distributions are built against the static MSVC runtime (`/MT`). Matching `/MT` prevents CRT heap mismatch bugs and DLL dependencies. |
| **Custom Local Scheme (`nexus://`)** | `http://localhost:<port>` | Local HTTP servers expose an open port on the host machine, making it vulnerable to local port scanning or cross-origin attacks. `nexus://` is an isolated standard scheme handled entirely in-memory by `AssetStore`. |
| **Single Shell Renderer** | Separate React app per tab | Running React in every tab consumes excessive RAM. A single persistent React shell for window controls + native content BrowserViews minimizes memory overhead. |
