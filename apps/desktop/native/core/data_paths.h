#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nexus::core {

// Everything NEXUS persists lives beneath this directory. CEF's own profile,
// caches and logs are redirected here so the browser never writes to the
// platform default CEF data directory.
struct LaunchOptions {
  std::filesystem::path data_root;
  bool no_sandbox_requested = false;
  std::string initial_url;
  bool smoke_test = false;
  std::string error;
};

// %LOCALAPPDATA%\NEXUS on Windows.
std::filesystem::path DefaultDataRoot();

// UTF-8 representation of a filesystem path, independent of the active
// Windows code page.
std::string PathToUtf8(const std::filesystem::path& path);

// Directory containing nexus.exe (and the staged ui/ assets).
std::filesystem::path ExecutableDirectory();

// Parses only NEXUS-owned switches. All other Chromium switches are ignored
// because CefSettings.command_line_args_disabled is enabled.
//
// Precedence: --user-data-dir / --nexus-user-data-dir > NEXUS_USER_DATA_DIR >
// default directory.
LaunchOptions ParseLaunchOptions(const std::vector<std::wstring>& args,
                                 const std::wstring& env_user_data_dir);

// Creates the local data layout. Returns false and fills |error| on failure.
bool EnsureDataLayout(const std::filesystem::path& root, std::string* error);

}  // namespace nexus::core
