#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_values.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "src/privacy_service.h"

namespace nexus {

class IpcRouter;

// Owns the native window, the permanent shell BrowserView (nexus://app), and
// one BrowserView per content tab. All methods are called on the CEF UI
// thread unless noted otherwise.
class BrowserWindow final : public CefWindowDelegate {
 public:
  BrowserWindow(CefRefPtr<PrivacyService> privacy,
                std::filesystem::path data_root,
                std::string initial_url = {},
                bool smoke_test = false);
  ~BrowserWindow() override;

  void Show();

  // CefWindowDelegate
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  void OnWindowBoundsChanged(CefRefPtr<CefWindow> window,
                             const CefRect& new_bounds) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;
  CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;

  // Shell lifecycle and trusted IPC.
  bool IsShellBrowser(const CefRefPtr<CefBrowser>& browser) const;
  void OnShellBrowserCreated(CefRefPtr<CefBrowser> browser);
  void OnShellBrowserClosed();
  void OnShellLoaded();
  bool OnShellProcessMessage(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefProcessId source_process,
                             CefRefPtr<CefProcessMessage> message);
  void SendIpcJson(const std::string& json);

  // Content tab lifecycle. These are invoked by ContentClient.
  void OnTabBrowserCreated(int tab_id, CefRefPtr<CefBrowser> browser);
  void OnTabBrowserClosed(int tab_id);
  void OnTabUrl(int tab_id, const std::string& url);
  void OnTabTitle(int tab_id, const std::string& title);
  void OnTabLoadingState(int tab_id,
                         bool loading,
                         bool can_go_back,
                         bool can_go_forward);
  void OnTabLoadError(int tab_id,
                      int error_code,
                      const std::string& error_text);
  void OnTabRenderProcessTerminated(int tab_id);
  void OpenUrlInNewTab(const std::string& url, bool foreground);

  // IPC surface.
  CefRefPtr<CefDictionaryValue> BuildAppInfo() const;
  CefRefPtr<CefDictionaryValue> BuildBrowserState() const;
  CefRefPtr<CefDictionaryValue> BuildPrivacyState() const;
  int CreateTab(const std::string& url, bool activate);
  bool CloseTab(int tab_id);
  bool ActivateTab(int tab_id);
  bool NavigateActive(const std::string& url);
  bool GoBack();
  bool GoForward();
  bool Reload();
  bool Stop();
  void SetDashboardOpen(bool open);
  void FocusAddressBar();
  void SetCookieMode(CookieMode mode);
  bool StartClearCookies();
  void OnCookiesCleared();

 private:
  struct Tab {
    int id = 0;
    std::string url;
    std::string title;
    bool loading = false;
    bool can_go_back = false;
    bool can_go_forward = false;
    int error_code = 0;
    std::string error_text;
    CefRefPtr<CefBrowser> browser;
    CefRefPtr<CefBrowserView> view;
  };

  Tab* FindTab(int tab_id);
  const Tab* FindTab(int tab_id) const;
  Tab* ActiveTab();
  CefRefPtr<CefBrowser> ActiveBrowser();
  void LayoutViews();
  void UpdateVisibility();
  void UpdateWindowTitle();
  void BroadcastState();
  void BroadcastPrivacyState();
  void SendEvent(const std::string& name,
                 CefRefPtr<CefDictionaryValue> payload);
  void ApplyDarkTitleBar();

  CefRefPtr<PrivacyService> privacy_;
  const std::filesystem::path data_root_;
  std::unique_ptr<IpcRouter> ipc_router_;

  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefBrowser> shell_browser_;
  CefRefPtr<CefBrowserView> shell_view_;
  bool shell_loaded_ = false;

  std::vector<Tab> tabs_;
  int next_tab_id_ = 1;
  int active_tab_id_ = 0;
  bool dashboard_open_ = false;
  std::string initial_url_;
  bool smoke_test_ = false;

  IMPLEMENT_REFCOUNTING(BrowserWindow);
  DISALLOW_COPY_AND_ASSIGN(BrowserWindow);
};

}  // namespace nexus
