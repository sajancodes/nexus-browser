#include "src/settings.h"

#include "core/data_paths.h"

namespace nexus {

CefSettings BuildCefSettings(const std::filesystem::path& data_root,
                             bool cef_logging) {
  CefSettings settings;

  // Security is not configurable by the user or by web content.
  settings.no_sandbox = false;
  settings.command_line_args_disabled = true;
  settings.multi_threaded_message_loop = false;
  settings.external_message_pump = false;
  settings.windowless_rendering_enabled = false;

  // Do not persist session cookies across runs of the browser process.
  settings.persist_session_cookies = false;

  // No CEF log file is written by default. NEXUS_CEF_LOG=1 enables a local
  // diagnostic log under the data directory for development builds.
  if (cef_logging) {
    settings.log_severity = LOGSEVERITY_INFO;
    CefString(&settings.log_file)
        .FromString(core::PathToUtf8(data_root / L"logs" / L"cef.log"));
  } else {
    settings.log_severity = LOGSEVERITY_DISABLE;
  }

  // Paint the pre-document background in the NEXUS shell colour so that page
  // transitions do not flash white.
  settings.background_color = 0xFF111315;

  const std::filesystem::path cef_root = data_root / L"cef";
  const std::filesystem::path cef_cache = cef_root / L"cache";

  CefString(&settings.root_cache_path)
      .FromString(core::PathToUtf8(cef_root));
  CefString(&settings.cache_path)
      .FromString(core::PathToUtf8(cef_cache));
  CefString(&settings.locale).FromASCII("en-US");

  return settings;
}

}  // namespace nexus
