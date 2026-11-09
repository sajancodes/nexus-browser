# NEXUS Security Specification & Threat Model

**Version:** 0.1.0  
**Security Baseline:** Multi-Process Sandboxed CEF with Isolated Local IPC  

Security is treated as an immutable structural constraint in NEXUS. We do not disable security sandboxes or degrade browser isolation boundaries to reduce memory footprint or accelerate startup.

---

## 1. Process Sandboxing & Isolation

NEXUS enforces Chromium's process isolation model:

- **Sandbox Broker:** The Windows sandbox broker initializes in `nexus.exe` (CEF bootstrap). If the sandbox token or initialization fails, the process exits with an error code rather than falling back to an un-sandboxed mode.
- **LPAC (Low-Privilege AppContainer):** Renderer, GPU, and network processes operate inside Windows AppContainers (`*S-1-15-2-2`). Renderers have zero access to the user's filesystem, network stack, or registry outside staged read-only runtime binaries.
- **Site Isolation:** Chromium's out-of-process iframes (OOPIF) and origin boundaries are maintained to prevent Spectre and cross-site data theft.
- **Single-Process Prohibition:** Running in single-process mode (`--single-process`) is forbidden in production builds.

---

## 2. Origin & Custom Scheme Security

NEXUS introduces a custom local scheme: `nexus://`.

### 2.1 Scheme Options Registration

`nexus://` is registered with CEF security flags:
```cpp
registrar->AddCustomScheme(
    "nexus",
    CEF_SCHEME_OPTION_STANDARD |
    CEF_SCHEME_OPTION_SECURE |
    CEF_SCHEME_OPTION_DISPLAY_ISOLATED |
    CEF_SCHEME_OPTION_CORS_ENABLED |
    CEF_SCHEME_OPTION_FETCH_ENABLED);
```

### 2.2 Host Isolation

- `nexus://app/*` — Serves the trusted UI shell.
- `nexus://newtab/*` — Serves the local new-tab page.
- Any attempt by web content to navigate to `nexus://app/` is blocked by `OnBeforeBrowse` and `ResolveNavigationInput`.

### 2.3 Content Security Policy (CSP)

- **UI Shell (`nexus://app/`):**
  ```text
  default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; connect-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'; object-src 'none'
  ```
  Zero remote network calls, zero inline scripts, zero third-party fonts or images.
- **New Tab Page (`nexus://newtab/`):**
  ```text
  default-src 'none'; script-src 'self'; style-src 'self'; img-src 'none'; font-src 'self'; connect-src 'none'; base-uri 'none'; form-action https://duckduckgo.com; frame-ancestors 'none'; object-src 'none'
  ```

---

## 3. Native IPC Threat Surface

Web content running in a browser is inherently untrusted. NEXUS eliminates renderer-to-native privilege escalation through three defense rings:

1. **V8 Ingestion Guard:** The `__nexusNative` bridge is only created in the main frame of `nexus://app/*` browsers with the explicit `role: "shell"` tag.
2. **IPC Message Guard:** In `IpcRouter::HandleMessage`, every incoming JSON message is checked for:
   - Valid JSON syntax and maximum size limit (64 KB).
   - Expected protocol version (`v == 1`).
   - Allowlisted method names.
   - Non-empty IDs and rate limits.
3. **No Raw Filesystem / Shell Execution:** The IPC surface provides no method to execute shell commands, open arbitary files, or write to arbitrary filesystem locations.

---

## 4. Web Permissions & Hardware Access

All web permission requests are rejected by default without user interruption:

```cpp
bool ContentClient::OnRequestMediaAccessPermission(...) {
  privacy_->RecordDeniedPermission();
  callback->Cancel();
  return true;
}

bool ContentClient::OnShowPermissionPrompt(...) {
  privacy_->RecordDeniedPermission();
  callback->Continue(CEF_PERMISSION_RESULT_DENY);
  return true;
}
```

- Camera & Microphone: Denied.
- Geolocation: Denied.
- Notifications: Denied.
- Web MIDI / USB / HID: Denied.
- Popups (`window.open`): Denied (`OnBeforePopup` returns `true`).

---

## 5. Transport Security & Certificate Validation

- **No Certificate Overrides:** `ContentClient::OnCertificateError` returns `false`. NEXUS never bypasses HTTPS certificate errors, expired certificates, or TLS handshake anomalies.
- **Strict HTTPS Upgrades:** User navigation input (`example.com`) resolves to `https://example.com` by default.
