#pragma once

#include <filesystem>

#include "include/cef_app.h"

namespace nexus {

// CEF is configured so that every persistent artefact lives beneath
// |data_root|. The default platform CEF directory is never used.
//
// When |cef_logging| is true, CEF writes an informational log to
// <data_root>/logs/cef.log. It is off by default because URLs can appear in
// Chromium diagnostics; enable it only while debugging a local build.
CefSettings BuildCefSettings(const std::filesystem::path& data_root,
                             bool cef_logging);

}  // namespace nexus
