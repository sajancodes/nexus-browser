# Building NEXUS

## Supported Platforms

- **Windows 10/11 x64** (Primary development platform)
- **Linux x64** (Debian/Ubuntu AppImage targets in roadmap)
- **macOS Apple Silicon & Intel** (Roadmap)

---

## Toolchain Requirements

| Component | Minimum Version | Verified In Session |
|---|---|---|
| **C++ Compiler** | MSVC 19.38+ (VS 2022) | VS 2022 BuildTools (x64) |
| **CMake** | 3.25+ | 4.4.4 |
| **Ninja** | 1.10+ | 1.13.2 |
| **Node.js** | 20 LTS+ | v24.19.0 |
| **pnpm** | 9+ / 12+ | 12.18.0 (via `corepack` / `npx`) |
| **Python** | 3.9+ | 3.11.8 |
| **CEF Distribution** | 156.0.26+g8ea0c73+chromium-156.0.8078.88 | Verified with SHA-1 check |

---

## Build Steps

### 1. Fetch CEF Binaries

Run the pinned CEF fetch script. It verifies the cryptographic SHA-1 hash against the official Spotify CDN index:

```powershell
python scripts/fetch_cef.py
```

### 2. Build Frontend and Native Host

Use the provided build orchestration script (`scripts/build.ps1`):

```powershell
.\scripts\build.ps1 -Preset windows-x64-release
```

To skip the React build when iterating strictly on C++ files:
```powershell
.\scripts\build.ps1 -Preset windows-x64-release -SkipFrontend
```

### 3. Run Native Unit Tests

```powershell
ctest --test-dir build\windows-x64-release -C Release --output-on-failure
```

### 4. Run Automated End-to-End Smoke Test

```powershell
.\build\windows-x64-release\bin\nexus.exe --smoke-test --url=https://example.com
```
