# CEF (Chromium Embedded Framework)

NEXUS does not commit CEF binaries. Fetch the pinned distribution with:

```powershell
python scripts/fetch_cef.py
```

The fetch script downloads from the official CEF build service
(`cef-builds.spotifycdn.com`), verifies SHA-1 against a pinned value, rejects
path-traversal entries, and extracts into `third_party/cef/binary/`.

Pinned build: **156.0.26+g8ea0c73+chromium-156.0.8078.88**
Chromium: **156.0.8078.88** (stable channel).

CEF upgrades are deliberate security events, not routine dependency bumps:

1. Review the [CEF release notes](https://cef-builds.spotifycdn.com/index.json)
   and upstream Chromium security fixes.
2. Update `CEF_VERSION` and the archive checksum in `scripts/fetch_cef.py`.
3. Build, run the full native + integration test suite, and re-verify that
   sandboxing and site isolation remain enabled.
4. Record the change in `CHANGELOG.md` and `docs/adr/`.
