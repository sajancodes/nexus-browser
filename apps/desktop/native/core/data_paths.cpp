#include "core/data_paths.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwctype>
#include <string_view>
#include <system_error>

namespace nexus::core {
namespace {

std::wstring GetEnvironmentValue(const wchar_t* name) {
  const DWORD required = ::GetEnvironmentVariableW(name, nullptr, 0);
  if (required == 0) {
    return {};
  }

  std::wstring value(required, L'\0');
  const DWORD written =
      ::GetEnvironmentVariableW(name, value.data(), required);
  if (written == 0 || written >= required) {
    return {};
  }
  value.resize(written);
  return value;
}

bool StartsWith(const std::wstring& value, const std::wstring& prefix) {
  return value.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), value.begin());
}

std::wstring Trim(const std::wstring& value) {
  size_t start = 0;
  size_t end = value.size();
  while (start < end && std::iswspace(value[start]) != 0) {
    ++start;
  }
  while (end > start && std::iswspace(value[end - 1]) != 0) {
    --end;
  }
  return value.substr(start, end - start);
}

std::wstring StripSurroundingQuotes(std::wstring value) {
  value = Trim(value);
  if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

}  // namespace

std::string PathToUtf8(const std::filesystem::path& path) {
  const std::u8string value = path.u8string();
  return std::string(reinterpret_cast<const char*>(value.data()),
                     value.size());
}

std::filesystem::path DefaultDataRoot() {
  std::wstring local_app_data = GetEnvironmentValue(L"LOCALAPPDATA");

  if (local_app_data.empty()) {
    PWSTR known_path = nullptr;
    if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                         nullptr, &known_path))) {
      local_app_data = known_path;
    }
    if (known_path != nullptr) {
      ::CoTaskMemFree(known_path);
    }
  }

  if (local_app_data.empty()) {
    // A last-resort fallback that still keeps data local to the machine.
    local_app_data = L".";
  }

  return std::filesystem::path(local_app_data) / L"NEXUS";
}

std::filesystem::path ExecutableDirectory() {
  std::vector<wchar_t> buffer(MAX_PATH);
  for (;;) {
    const DWORD length = ::GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return std::filesystem::current_path();
    }
    if (length < buffer.size() - 1) {
      return std::filesystem::path(
                 std::wstring(buffer.data(), buffer.data() + length))
          .parent_path();
    }
    buffer.resize(buffer.size() * 2);
  }
}

LaunchOptions ParseLaunchOptions(const std::vector<std::wstring>& args,
                                 const std::wstring& env_user_data_dir) {
  LaunchOptions options;
  std::wstring override_path;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::wstring& arg = args[i];

    if (arg == L"--no-sandbox" || StartsWith(arg, L"--no-sandbox=")) {
      options.no_sandbox_requested = true;
      options.error =
          "NEXUS refuses to run with --no-sandbox. Sandboxing is not optional.";
      continue;
    }

    constexpr std::wstring_view kUserDataDir = L"--user-data-dir=";
    constexpr std::wstring_view kNexusUserDataDir = L"--nexus-user-data-dir=";

    if (arg == L"--user-data-dir") {
      if (i + 1 < args.size()) {
        override_path = args[++i];
      }
    } else if (arg.size() > kUserDataDir.size() &&
               arg.compare(0, kUserDataDir.size(), kUserDataDir) == 0) {
      override_path = arg.substr(kUserDataDir.size());
    } else if (arg == L"--nexus-user-data-dir") {
      if (i + 1 < args.size()) {
        override_path = args[++i];
      }
    } else if (arg.size() > kNexusUserDataDir.size() &&
               arg.compare(0, kNexusUserDataDir.size(), kNexusUserDataDir) ==
                   0) {
      override_path = arg.substr(kNexusUserDataDir.size());
    }
  }

  if (override_path.empty()) {
    override_path = env_user_data_dir;
  }

  override_path = StripSurroundingQuotes(override_path);
  const std::filesystem::path chosen =
      override_path.empty() ? DefaultDataRoot()
                            : std::filesystem::path(override_path);

  std::error_code ec;
  std::filesystem::path absolute = std::filesystem::absolute(chosen, ec);
  if (ec) {
    options.error = "Unable to resolve the NEXUS data directory.";
    return options;
  }

  std::filesystem::path normalized =
      std::filesystem::weakly_canonical(absolute, ec);
  if (ec) {
    normalized = absolute.lexically_normal();
  }

  if (normalized.empty() || normalized == normalized.root_path()) {
    options.error = "The NEXUS data directory may not be a filesystem root.";
    return options;
  }

  options.data_root = std::move(normalized);
  return options;
}

bool EnsureDataLayout(const std::filesystem::path& root, std::string* error) {
  const std::filesystem::path directories[] = {
      root,
      root / L"cef",
      root / L"cef" / L"cache",
      root / L"logs",
      root / L"profile",
  };

  for (const auto& directory : directories) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
      if (error != nullptr) {
        *error = "Unable to create " + PathToUtf8(directory) + ": " +
                 ec.message();
      }
      return false;
    }
  }

  return true;
}

}  // namespace nexus::core
