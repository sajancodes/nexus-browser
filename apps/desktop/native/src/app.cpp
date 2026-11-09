#include "src/app.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "core/data_paths.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"
#include "src/asset_store.h"
#include "src/browser_window.h"
#include "src/render_process_handler.h"
#include "src/scheme_handler.h"

namespace nexus {

NexusApp::NexusApp(std::filesystem::path data_root)
    : data_root_(std::move(data_root)) {}

NexusApp::~NexusApp() = default;

void NexusApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  if (!registrar) {
    return;
  }
  registrar->AddCustomScheme(
      "nexus",
      CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
          CEF_SCHEME_OPTION_DISPLAY_ISOLATED | CEF_SCHEME_OPTION_CORS_ENABLED |
          CEF_SCHEME_OPTION_FETCH_ENABLED);
}

CefRefPtr<CefBrowserProcessHandler> NexusApp::GetBrowserProcessHandler() {
  return this;
}

CefRefPtr<CefRenderProcessHandler> NexusApp::GetRenderProcessHandler() {
  if (!render_process_handler_) {
    render_process_handler_ = new NexusRenderProcessHandler();
  }
  return render_process_handler_;
}

void NexusApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  const std::filesystem::path ui_root = core::ExecutableDirectory() / L"ui";
  asset_store_ = std::make_shared<AssetStore>(ui_root);
  if (!asset_store_->ready()) {
    MessageBoxW(nullptr,
                L"The NEXUS shell assets are missing from the ui directory.\n"
                L"Build the frontend before launching the browser.",
                L"NEXUS startup error", MB_ICONERROR | MB_OK);
    CefQuitMessageLoop();
    return;
  }

  if (!CefRegisterSchemeHandlerFactory(
          "nexus", "",
          new NexusSchemeHandlerFactory(asset_store_))) {
    MessageBoxW(nullptr, L"Unable to register the local NEXUS scheme.",
                L"NEXUS startup error", MB_ICONERROR | MB_OK);
    CefQuitMessageLoop();
    return;
  }

  privacy_ = new PrivacyService();
  privacy_->LoadPreferences(data_root_ / L"profile" / L"preferences.json");

  window_ = new BrowserWindow(privacy_, data_root_);
  window_->Show();
}

}  // namespace nexus
