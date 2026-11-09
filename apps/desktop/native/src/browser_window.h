#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_download_item.h"
#include "include/cef_download_handler.h"
#include "include/cef_values.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "src/db/database.h"
#include "src/privacy_service.h"
#include "src/vpn/vpn_manager.h"

namespace nexus {

class IpcRouter;
class NativeChrome;

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
  int DuplicateTab(int tab_id);
  bool RestoreTab();
  bool ActivateTab(int tab_id);
  bool NavigateActive(const std::string& url);
  bool GoBack();
  bool GoForward();
  bool Reload();
  bool Stop();
  bool ReloadOrStop();
  bool NavigateFromAddressBar(const std::string& input);
  double ZoomIn();
  double ZoomOut();
  double ResetZoom();
  void FindInPage(const std::string& query, bool forward, bool find_next);
  void StopFinding(bool clear_selection);

  void SetDashboardOpen(bool open);
  void SetActivePanel(const std::string& panel);
  const std::string& active_panel() const { return active_panel_; }
  void FocusAddressBar();
  void SetCookieMode(CookieMode mode);
  bool StartClearCookies();
  void OnCookiesCleared();

  // Database & VPN access
  db::Database* database() { return db_.get(); }
  vpn::VpnManager* vpn() { return vpn_.get(); }
  const std::filesystem::path& data_root() const { return data_root_; }

  // Downloads
  bool CancelDownload(const std::string& guid);
  bool OpenDownloadedFile(const std::string& guid);
  bool OpenDownloadedFolder(const std::string& guid);
  void OnDownloadStarted(const db::DownloadRecord& record, CefRefPtr<CefDownloadItemCallback> callback);
  void OnDownloadUpdated(CefRefPtr<CefDownloadItem> item, CefRefPtr<CefDownloadItemCallback> callback);
  void BroadcastDownloads();

  // Media
  void OnMediaDetected(const db::MediaItem& item);

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
    double zoom_level = 0.0;
    CefRefPtr<CefBrowser> browser;
    CefRefPtr<CefBrowserView> view;
  };

  struct ClosedTab {
    std::string url;
    std::string title;
  };

  Tab* FindTab(int tab_id);
  const Tab* FindTab(int tab_id) const;
  Tab* ActiveTab();
  CefRefPtr<CefBrowser> ActiveBrowser();
  void LayoutViews();
  void UpdateVisibility();
  bool EnsureReactInterface();
  void SyncNativeChrome();
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
  CefRefPtr<NativeChrome> native_chrome_;
  CefRefPtr<CefBrowser> shell_browser_;
  CefRefPtr<CefBrowserView> shell_view_;
  bool shell_loaded_ = false;

  std::vector<Tab> tabs_;
  std::vector<ClosedTab> closed_tabs_;
  int next_tab_id_ = 1;
  int active_tab_id_ = 0;
  bool dashboard_open_ = false;
  std::string active_panel_ = "none";
  std::string initial_url_;
  bool smoke_test_ = false;

  std::shared_ptr<db::Database> db_;
  std::shared_ptr<vpn::VpnManager> vpn_;
  std::unordered_map<std::string, CefRefPtr<CefDownloadItemCallback>> active_downloads_;

  IMPLEMENT_REFCOUNTING(BrowserWindow);
  DISALLOW_COPY_AND_ASSIGN(BrowserWindow);
};

}  // namespace nexus
