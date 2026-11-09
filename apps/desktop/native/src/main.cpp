// NEXUS native entry point.
//
// Process layout mirrors CEF's supported Windows sandbox design:
//   nexus.exe (renamed CEF bootstrap.exe)
//     -> loads nexus.dll
//        -> RunWinMain(hInstance, ..., sandbox_info)
//           -> CefExecuteProcess for child processes
//           -> CefInitialize / CefRunMessageLoop / CefShutdown
//
// No --no-sandbox path exists in NEXUS. If the bootstrap fails to provide
// sandbox_info the application refuses to start rather than degrading security.

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/data_paths.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"
#include "src/app.h"
#include "src/settings.h"

namespace nexus {
namespace {

std::wstring EnvironmentValue(const wchar_t* name) {
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

std::vector<std::wstring> ProcessArguments() {
  int argument_count = 0;
  LPWSTR* arguments = ::CommandLineToArgvW(::GetCommandLineW(), &argument_count);
  if (!arguments) {
    return {};
  }

  std::vector<std::wstring> result;
  result.reserve(static_cast<size_t>(argument_count));
  for (int i = 0; i < argument_count; ++i) {
    result.emplace_back(arguments[i]);
  }
  ::LocalFree(arguments);
  return result;
}

std::wstring WidenUtf8(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                             text.data(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0);
  if (required <= 0) {
    return L"Unknown error.";
  }
  std::wstring wide(static_cast<size_t>(required), L'\0');
  ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), wide.data(), required);
  return wide;
}

void ShowStartupError(const std::wstring& message) {
  ::MessageBoxW(nullptr, message.c_str(), L"NEXUS startup error",
                MB_ICONERROR | MB_OK);
}

int RunMain(HINSTANCE hInstance,
            LPWSTR /*lpCmdLine*/,
            int /*nCmdShow*/,
            void* sandbox_info) {
  CefMainArgs main_args(hInstance);

  const core::LaunchOptions options = core::ParseLaunchOptions(
      ProcessArguments(), EnvironmentValue(L"NEXUS_USER_DATA_DIR"));
  if (!options.error.empty()) {
    ShowStartupError(WidenUtf8(options.error));
    return 1;
  }

  CefRefPtr<NexusApp> app = new NexusApp(options);

  // Child processes (renderer, GPU, utility) execute and exit here.
  const int process_exit_code =
      CefExecuteProcess(main_args, app, sandbox_info);
  if (process_exit_code >= 0) {
    return process_exit_code;
  }

  // The sandbox information must be present in the browser process. Refusing
  // to continue is deliberate: starting without it would silently run
  // renderers without the sandbox.
  if (!sandbox_info) {
    ShowStartupError(
        L"The CEF sandbox broker did not initialize. NEXUS will not start "
        L"without sandbox protection.");
    return 1;
  }

  std::string layout_error;
  if (!core::EnsureDataLayout(options.data_root, &layout_error)) {
    ShowStartupError(WidenUtf8(layout_error));
    return 1;
  }

  const CefSettings settings = BuildCefSettings(
      options.data_root, EnvironmentValue(L"NEXUS_CEF_LOG") == L"1");
  if (!CefInitialize(main_args, settings, app.get(), sandbox_info)) {
    ShowStartupError(L"Chromium could not initialize. NEXUS will exit.");
    return CefGetExitCode();
  }

  CefRunMessageLoop();
  CefShutdown();
  return 0;
}

}  // namespace
}  // namespace nexus

#if defined(CEF_USE_BOOTSTRAP)
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE hInstance,
                                    LPWSTR lpCmdLine,
                                    int nCmdShow,
                                    void* sandbox_info,
                                    cef_version_info_t* /*version_info*/) {
  return nexus::RunMain(hInstance, lpCmdLine, nCmdShow, sandbox_info);
}
#else
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  return nexus::RunMain(hInstance, lpCmdLine, nCmdShow, nullptr);
}
#endif
