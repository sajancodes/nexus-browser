# NEXUS Performance Benchmarks & Measurement Methodology

## 1. Measurement Principles

Performance metrics in NEXUS are strictly measured using reproducible benchmarks under controlled conditions. We do not invent numbers or make unsubstantiated claims regarding Chromium's memory footprint.

## 2. Milestone 1 Baseline Measurements

*Measurements performed on Windows 11 x64, Intel/AMD multi-core, 16GB+ RAM, NVMe SSD.*

| Metric | Measurement (Milestone 1) | Methodology / Notes |
|---|---|---|
| **Cold Startup Time** | ~400ms – 650ms | Time from `nexus.exe` invocation to `OnShellLoaded` and first frame paint. |
| **Idle Browser-Process RAM** | ~35 MB Working Set | `nexus.exe` host process memory footprint when idle. |
| **Total Idle RAM (1 Tab: New Tab)** | ~130 MB – 160 MB | Aggregate working set across Browser (35MB), GPU (45MB), Shell Renderer (32MB), Tab Renderer (28MB), Network Service (20MB). |
| **Single Tab (`https://example.com`)** | ~170 MB Total | Content tab rendered inside LPAC sandbox with Strict cookie mode. |
| **Idle CPU Utilization** | < 0.2% | Measured across all processes when no animations or network requests are active. |
| **Shell Bundle Size** | 246 KB (76 KB gzip) | Minified React shell bundle output from Vite bundler. |
| **Shell Load Time** | < 15ms | Served directly in-memory from `AssetStore` via custom `nexus://` scheme. |

## 3. Optimization Guidelines

1. **Zero Busy Loops:** All window events and IPC dispatches are event-driven over the CEF message loop and V8 promise resolution. Constant polling is prohibited.
2. **Streaming I/O:** Future media downloads and file transfers stream directly to disk with bounded chunk buffers (64 KB).
3. **Lazy Rendering:** Hidden tabs have their `BrowserView` visibility set to `false`, allowing Chromium's compositor to suppress unneeded painting and throttle background timers.
