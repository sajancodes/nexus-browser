#pragma once

#include <filesystem>
#include <memory>

#include "core/data_paths.h"
#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "src/privacy_service.h"

namespace nexus {

class AssetStore;
class BrowserWindow;
class NexusRenderProcessHandler;

// Browser-process application object. The renderer-side handler is returned
// for child processes; no persistent state is touched before CefExecuteProcess
// decides whether this is a child process.
class NexusApp final : public CefApp, public CefBrowserProcessHandler {
 public:
  explicit NexusApp(core::LaunchOptions options);
  ~NexusApp() override;

  // CefApp
  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

  // CefBrowserProcessHandler
  void OnContextInitialized() override;

 private:
  const core::LaunchOptions options_;
  std::shared_ptr<AssetStore> asset_store_;
  CefRefPtr<PrivacyService> privacy_;
  CefRefPtr<BrowserWindow> window_;
  CefRefPtr<NexusRenderProcessHandler> render_process_handler_;

  IMPLEMENT_REFCOUNTING(NexusApp);
  DISALLOW_COPY_AND_ASSIGN(NexusApp);
};

}  // namespace nexus
