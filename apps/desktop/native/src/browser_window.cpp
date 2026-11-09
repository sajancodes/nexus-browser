#include "src/browser_window.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

#include "core/data_paths.h"
#include "core/url_utils.h"
#include "include/cef_app.h"
#include "include/cef_cookie.h"
#include "include/cef_parser.h"
#include "include/cef_process_message.h"
#include "include/cef_version.h"
#include "include/wrapper/cef_helpers.h"
#include "src/content_client.h"
#include "src/ipc_router.h"
#include "src/shell_client.h"

#ifndef NEXUS_VERSION
#define NEXUS_VERSION "0.1.0-dev"
#endif

namespace nexus {
namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 800;
constexpr int kShellHeight = 88;
constexpr cef_color_t kWindowBackground = 0xFF111315;

constexpr int kCommandFocusAddress = 1001;
constexpr int kCommandNewTab = 1002;
constexpr int kCommandCloseTab = 1003;
constexpr int kCommandReload = 1004;
constexpr int kCommandToggleDashboard = 1005;
constexpr int kCommandBack = 1006;
constexpr int kCommandForward = 1007;
constexpr int kCommandRestoreTab = 1008;
constexpr int kCommandBookmark = 1009;
constexpr int kCommandZoomIn = 1010;
constexpr int kCommandZoomOut = 1011;
constexpr int kCommandZoomReset = 1012;

constexpr char kNewTabUrl[] = "nexus://newtab/";
constexpr char kShellUrl[] = "nexus://app/";

constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
constexpr DWORD kDwmwaCaptionColor = 35;
constexpr DWORD kDwmwaTextColor = 36;

class NexusBrowserViewDelegate final : public CefBrowserViewDelegate {
 public:
  NexusBrowserViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(NexusBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(NexusBrowserViewDelegate);
};

std::string InitialTitleForUrl(const std::string& url) {
  if (core::IsInternalNewTabUrl(url)) {
    return "New Tab";
  }
  const std::string host = core::HostOfUrl(url);
  return host.empty() ? url : host;
}

std::string ChromiumVersion() {
  return std::to_string(CHROME_VERSION_MAJOR) + "." +
         std::to_string(CHROME_VERSION_MINOR) + "." +
         std::to_string(CHROME_VERSION_BUILD) + "." +
         std::to_string(CHROME_VERSION_PATCH);
}

class ClearCookiesCallback final : public CefDeleteCookiesCallback {
 public:
  ClearCookiesCallback(CefRefPtr<BrowserWindow> window,
                       CefRefPtr<PrivacyService> privacy)
      : window_(std::move(window)), privacy_(std::move(privacy)) {}

  void OnComplete(int num_deleted) override {
    privacy_->RecordDeletedCookies(num_deleted);
    window_->OnCookiesCleared();
  }

 private:
  CefRefPtr<BrowserWindow> window_;
  CefRefPtr<PrivacyService> privacy_;

  IMPLEMENT_REFCOUNTING(ClearCookiesCallback);
  DISALLOW_COPY_AND_ASSIGN(ClearCookiesCallback);
};

}  // namespace

BrowserWindow::BrowserWindow(CefRefPtr<PrivacyService> privacy,
                             std::filesystem::path data_root,
                             std::string initial_url,
                             bool smoke_test)
    : privacy_(std::move(privacy)),
      data_root_(std::move(data_root)),
      ipc_router_(std::make_unique<IpcRouter>(this)),
      initial_url_(std::move(initial_url)),
      smoke_test_(smoke_test) {
  db_ = std::make_shared<db::Database>();
  db_->Open(data_root_ / L"profile" / L"nexus.db");
  vpn_ = std::make_shared<vpn::VpnManager>();
  if (privacy_ && privacy_->filter_engine()) {
    privacy_->filter_engine()->LoadDatabaseRules(db_->GetFilterRules());
  }
}

BrowserWindow::~BrowserWindow() = default;

void BrowserWindow::Show() {
  CefWindow::CreateTopLevelWindow(this);
}

CefRect BrowserWindow::GetInitialBounds(CefRefPtr<CefWindow> /*window*/) {
  return CefRect(0, 0, kInitialWindowWidth, kInitialWindowHeight);
}

void BrowserWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  window_ = window;

  window_->SetTitle("NEXUS");
  window_->SetBackgroundColor(kWindowBackground);

  window_->SetAccelerator(kCommandFocusAddress, 'L', false, true, false, true);
  window_->SetAccelerator(kCommandNewTab, 'T', false, true, false, true);
  window_->SetAccelerator(kCommandRestoreTab, 'T', true, true, false, true);
  window_->SetAccelerator(kCommandCloseTab, 'W', false, true, false, true);
  window_->SetAccelerator(kCommandReload, 'R', false, true, false, true);
  window_->SetAccelerator(kCommandToggleDashboard, 'P', true, true, false, true);
  window_->SetAccelerator(kCommandBack, VK_LEFT, false, false, true, true);
  window_->SetAccelerator(kCommandForward, VK_RIGHT, false, false, true, true);
  window_->SetAccelerator(kCommandZoomIn, VK_OEM_PLUS, false, true, false, true);
  window_->SetAccelerator(kCommandZoomOut, VK_OEM_MINUS, false, true, false, true);
  window_->SetAccelerator(kCommandZoomReset, '0', false, true, false, true);

  CefBrowserSettings shell_settings;
  shell_settings.background_color = kWindowBackground;
  CefRefPtr<CefDictionaryValue> shell_extra = CefDictionaryValue::Create();
  shell_extra->SetString("role", "shell");
  shell_view_ = CefBrowserView::CreateBrowserView(
      new ShellClient(this), CefString(kShellUrl), shell_settings, shell_extra,
      nullptr, new NexusBrowserViewDelegate());
  window_->AddChildView(shell_view_);

  window_->CenterWindow(CefSize(kInitialWindowWidth, kInitialWindowHeight));
  window_->Show();
  ApplyDarkTitleBar();

  const std::string start_url =
      initial_url_.empty() ? std::string(kNewTabUrl) : initial_url_;
  CreateTab(start_url, true);
  std::fprintf(stderr, "NEXUS OnWindowCreated: shell_view=%p tabs=%zu active=%d start_url=%s\n",
               static_cast<void*>(shell_view_.get()), tabs_.size(),
               active_tab_id_, start_url.c_str());
  std::fflush(stderr);
  LayoutViews();
}

void BrowserWindow::OnWindowDestroyed(CefRefPtr<CefWindow> /*window*/) {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS OnWindowDestroyed\\n");
  std::fflush(stderr);
  shell_loaded_ = false;
  shell_view_ = nullptr;
  shell_browser_ = nullptr;
  tabs_.clear();
  window_ = nullptr;
  CefQuitMessageLoop();
}

void BrowserWindow::OnWindowBoundsChanged(CefRefPtr<CefWindow> /*window*/,
                                          const CefRect& /*new_bounds*/) {
  CEF_REQUIRE_UI_THREAD();
  LayoutViews();
}

bool BrowserWindow::CanClose(CefRefPtr<CefWindow> /*window*/) {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS CanClose called\\n");
  std::fflush(stderr);
  // Force-close tab browsers so a page's beforeunload handler cannot keep the
  // NEXUS window alive after the user asks to exit.
  for (Tab& tab : tabs_) {
    if (tab.browser) {
      tab.browser->GetHost()->CloseBrowser(true);
    }
  }
  if (shell_browser_) {
    shell_browser_->GetHost()->CloseBrowser(true);
  }
  return true;
}

bool BrowserWindow::OnAccelerator(CefRefPtr<CefWindow> /*window*/,
                                  int command_id) {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS OnAccelerator command=%d\n", command_id);
  std::fflush(stderr);
  switch (command_id) {
    case kCommandFocusAddress:
      FocusAddressBar();
      return true;
    case kCommandNewTab:
      CreateTab(kNewTabUrl, true);
      FocusAddressBar();
      return true;
    case kCommandRestoreTab:
      RestoreTab();
      return true;
    case kCommandCloseTab: {
      Tab* tab = ActiveTab();
      if (tab) {
        return CloseTab(tab->id);
      }
      return false;
    }
    case kCommandReload:
      return Reload();
    case kCommandToggleDashboard:
      SetDashboardOpen(!dashboard_open_);
      return true;
    case kCommandBack:
      return GoBack();
    case kCommandForward:
      return GoForward();
    case kCommandZoomIn:
      ZoomIn();
      return true;
    case kCommandZoomOut:
      ZoomOut();
      return true;
    case kCommandZoomReset:
      ResetZoom();
      return true;
    default:
      return false;
  }
}

bool BrowserWindow::IsShellBrowser(
    const CefRefPtr<CefBrowser>& browser) const {
  return shell_browser_ && browser &&
         shell_browser_->GetIdentifier() == browser->GetIdentifier();
}

void BrowserWindow::OnShellBrowserCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  shell_browser_ = std::move(browser);
  if (shell_view_) {
    shell_view_->SetBackgroundColor(kWindowBackground);
  }
  LayoutViews();
}

void BrowserWindow::OnShellBrowserClosed() {
  CEF_REQUIRE_UI_THREAD();
  shell_loaded_ = false;
  shell_browser_ = nullptr;
  shell_view_ = nullptr;
}

void BrowserWindow::OnShellLoaded() {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS OnShellLoaded\\n");
  std::fflush(stderr);
  shell_loaded_ = true;
  LayoutViews();
  BroadcastState();
  BroadcastPrivacyState();
  SendEvent("ui.focusAddress", CefDictionaryValue::Create());
}

bool BrowserWindow::OnShellProcessMessage(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();
  return ipc_router_->HandleMessage(std::move(browser), std::move(frame),
                                    source_process, std::move(message));
}

void BrowserWindow::SendIpcJson(const std::string& json) {
  CEF_REQUIRE_UI_THREAD();
  if (!shell_browser_) {
    return;
  }
  CefRefPtr<CefFrame> frame = shell_browser_->GetMainFrame();
  if (!frame) {
    return;
  }
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create("nexus.ipc.message");
  if (!message || !message->IsValid()) {
    return;
  }
  message->GetArgumentList()->SetString(0, CefString(json));
  frame->SendProcessMessage(PID_RENDERER, message);
}

void BrowserWindow::OnTabBrowserCreated(int tab_id,
                                        CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS OnTabBrowserCreated tab=%d\\n", tab_id);
  std::fflush(stderr);
  Tab* tab = FindTab(tab_id);
  if (!tab) {
    return;
  }
  tab->browser = std::move(browser);
  if (tab->browser) {
    tab->view = CefBrowserView::GetForBrowser(tab->browser);
  }
  LayoutViews();
  UpdateVisibility();
  BroadcastState();
}

void BrowserWindow::OnTabBrowserClosed(int tab_id) {
  CEF_REQUIRE_UI_THREAD();
  int index = -1;
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == tab_id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return;
  }

  const bool was_active = active_tab_id_ == tab_id;
  tabs_.erase(tabs_.begin() + index);
  if (was_active) {
    if (tabs_.empty()) {
      active_tab_id_ = 0;
    } else {
      const size_t next = std::min(static_cast<size_t>(index),
                                   tabs_.size() - 1);
      active_tab_id_ = tabs_[next].id;
    }
  }
  UpdateVisibility();
  LayoutViews();
  UpdateWindowTitle();
  BroadcastState();
}

void BrowserWindow::OnTabUrl(int tab_id, const std::string& url) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab || tab->url == url) {
    return;
  }
  tab->url = url;
  tab->error_code = 0;
  tab->error_text.clear();
  BroadcastState();
}

void BrowserWindow::OnTabTitle(int tab_id, const std::string& title) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab || tab->title == title) {
    return;
  }
  tab->title = title;
  if (active_tab_id_ == tab_id) {
    UpdateWindowTitle();
  }
  BroadcastState();
}

void BrowserWindow::OnTabLoadingState(int tab_id,
                                      bool loading,
                                      bool can_go_back,
                                      bool can_go_forward) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab) {
    return;
  }
  tab->loading = loading;
  tab->can_go_back = can_go_back;
  tab->can_go_forward = can_go_forward;

  if (!loading && db_ && !tab->url.empty() &&
      !core::IsInternalNewTabUrl(tab->url) && !core::IsInternalAppUrl(tab->url)) {
    db_->AddHistory(tab->url, tab->title.empty() ? tab->url : tab->title);
  }

  BroadcastState();

  if (!loading && smoke_test_) {
    std::fprintf(stderr, "NEXUS_SMOKE_TEST_PASSED: %s title='%s'\n",
                 tab->url.c_str(), tab->title.c_str());
    std::fflush(stderr);
    if (window_) {
      window_->Close();
    }
  }
}

void BrowserWindow::OnTabLoadError(int tab_id,
                                   int error_code,
                                   const std::string& error_text) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab) {
    return;
  }
  tab->loading = false;
  tab->error_code = error_code;
  tab->error_text = error_text;
  BroadcastState();

  if (smoke_test_) {
    std::fprintf(stderr, "NEXUS_SMOKE_TEST_FAILED: %s error=%d (%s)\n",
                 tab->url.c_str(), error_code, error_text.c_str());
    std::fflush(stderr);
    if (window_) {
      window_->Close();
    }
  }
}

void BrowserWindow::OnTabRenderProcessTerminated(int tab_id) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab) {
    return;
  }
  tab->loading = false;
  tab->error_code = -1;
  tab->error_text = "The page's process stopped unexpectedly.";
  BroadcastState();
}

void BrowserWindow::OpenUrlInNewTab(const std::string& url, bool foreground) {
  CEF_REQUIRE_UI_THREAD();
  CreateTab(url, foreground);
}

CefRefPtr<CefDictionaryValue> BrowserWindow::BuildAppInfo() const {
  CefRefPtr<CefDictionaryValue> info = CefDictionaryValue::Create();
  info->SetString("name", "NEXUS");
  info->SetString("version", NEXUS_VERSION);
  info->SetString("platform", "Windows x64");
  info->SetString("cefVersion", CEF_VERSION);
  info->SetString("chromiumVersion", CefString(ChromiumVersion()));
  info->SetString("dataDir", CefString(core::PathToUtf8(data_root_)));
  info->SetBool("sandbox", true);
  info->SetInt("milestone", 8);
  return info;
}

CefRefPtr<CefDictionaryValue> BrowserWindow::BuildBrowserState() const {
  CefRefPtr<CefDictionaryValue> state = CefDictionaryValue::Create();
  CefRefPtr<CefListValue> list = CefListValue::Create();

  for (size_t i = 0; i < tabs_.size(); ++i) {
    const Tab& tab = tabs_[i];
    CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
    item->SetInt("id", tab.id);
    item->SetString("title", CefString(tab.title));
    item->SetString("url", CefString(tab.url));
    item->SetBool("loading", tab.loading);
    item->SetBool("canGoBack", tab.can_go_back);
    item->SetBool("canGoForward", tab.can_go_forward);
    item->SetInt("errorCode", tab.error_code);
    item->SetString("errorText", CefString(tab.error_text));
    item->SetBool("secure", core::IsHttpsUrl(tab.url));
    item->SetBool("internal", core::IsInternalNewTabUrl(tab.url));
    item->SetDouble("zoomLevel", tab.zoom_level);
    list->SetDictionary(static_cast<size_t>(i), item);
  }

  state->SetList("tabs", list);
  state->SetInt("activeTabId", active_tab_id_);
  state->SetBool("dashboardOpen", dashboard_open_);
  state->SetString("activePanel", CefString(active_panel_));
  return state;
}

CefRefPtr<CefDictionaryValue> BrowserWindow::BuildPrivacyState() const {
  CefRefPtr<CefDictionaryValue> state = CefDictionaryValue::Create();
  state->SetString("cookieMode",
                   PrivacyService::CookieModeToString(privacy_->mode()));
  state->SetDouble("blockedTrackers",
                   static_cast<double>(privacy_->blocked_trackers()));
  state->SetDouble("blockedCookies",
                   static_cast<double>(privacy_->blocked_cookies()));
  state->SetDouble("deniedPermissions",
                   static_cast<double>(privacy_->denied_permissions()));
  state->SetDouble("blockedDownloads",
                   static_cast<double>(privacy_->blocked_downloads()));
  state->SetDouble("strippedReferrers",
                   static_cast<double>(privacy_->stripped_referrers()));
  state->SetDouble("blockedPopups",
                   static_cast<double>(privacy_->blocked_popups()));
  state->SetDouble("blockedNavigations",
                   static_cast<double>(privacy_->blocked_navigations()));
  state->SetDouble("deletedCookies",
                   static_cast<double>(privacy_->deleted_cookies()));
  return state;
}

int BrowserWindow::CreateTab(const std::string& url, bool activate) {
  CEF_REQUIRE_UI_THREAD();

  std::string target = url;
  if (!core::IsAllowedContentUrl(target) || target.size() > 8192) {
    target = kNewTabUrl;
  }

  const int tab_id = next_tab_id_++;

  Tab tab;
  tab.id = tab_id;
  tab.url = target;
  tab.title = InitialTitleForUrl(target);
  tabs_.push_back(std::move(tab));

  if (activate || active_tab_id_ == 0) {
    active_tab_id_ = tab_id;
  }

  if (window_) {
    CefBrowserSettings settings;
    settings.background_color = 0xFF181B1F;

    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    extra_info->SetString("role", "content");
    extra_info->SetInt("tabId", tab_id);

    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new ContentClient(this, tab_id, privacy_), CefString(target), settings,
        extra_info, nullptr, new NexusBrowserViewDelegate());

    Tab* created = FindTab(tab_id);
    if (created && view) {
      created->view = view;
      window_->AddChildView(view);
    }
  }

  LayoutViews();
  UpdateVisibility();
  UpdateWindowTitle();
  BroadcastState();
  return tab_id;
}

bool BrowserWindow::CloseTab(int tab_id) {
  CEF_REQUIRE_UI_THREAD();
  int index = -1;
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == tab_id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return false;
  }

  if (!core::IsInternalNewTabUrl(tabs_[index].url)) {
    closed_tabs_.push_back({tabs_[index].url, tabs_[index].title});
    if (closed_tabs_.size() > 25) {
      closed_tabs_.erase(closed_tabs_.begin());
    }
  }

  if (tabs_.size() == 1) {
    if (window_) {
      window_->Close();
    }
    return true;
  }

  int next_active = active_tab_id_;
  if (active_tab_id_ == tab_id) {
    if (static_cast<size_t>(index + 1) < tabs_.size()) {
      next_active = tabs_[index + 1].id;
    } else {
      next_active = tabs_[static_cast<size_t>(index - 1)].id;
    }
  }

  CefRefPtr<CefBrowserView> view = tabs_[static_cast<size_t>(index)].view;
  CefRefPtr<CefBrowser> browser = tabs_[static_cast<size_t>(index)].browser;
  tabs_.erase(tabs_.begin() + index);

  if (window_ && view) {
    window_->RemoveChildView(view);
  }
  if (browser) {
    browser->GetHost()->CloseBrowser(true);
  }

  active_tab_id_ = next_active;
  UpdateVisibility();
  LayoutViews();
  UpdateWindowTitle();
  BroadcastState();
  return true;
}

int BrowserWindow::DuplicateTab(int tab_id) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab) {
    return 0;
  }
  return CreateTab(tab->url, true);
}

bool BrowserWindow::RestoreTab() {
  CEF_REQUIRE_UI_THREAD();
  if (closed_tabs_.empty()) {
    return false;
  }
  ClosedTab last = closed_tabs_.back();
  closed_tabs_.pop_back();
  CreateTab(last.url, true);
  return true;
}

bool BrowserWindow::ActivateTab(int tab_id) {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = FindTab(tab_id);
  if (!tab) {
    return false;
  }
  active_tab_id_ = tab_id;
  UpdateVisibility();
  LayoutViews();
  UpdateWindowTitle();
  if (!dashboard_open_ && tab->view) {
    tab->view->RequestFocus();
  }
  BroadcastState();
  return true;
}

bool BrowserWindow::NavigateActive(const std::string& url) {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS NavigateActive url=%s\n", url.c_str());
  std::fflush(stderr);
  if (!core::IsAllowedContentUrl(url)) {
    return false;
  }
  Tab* tab = ActiveTab();
  if (!tab || !tab->browser) {
    return false;
  }
  CefRefPtr<CefFrame> frame = tab->browser->GetMainFrame();
  if (!frame) {
    return false;
  }
  tab->error_code = 0;
  tab->error_text.clear();
  tab->url = url;
  frame->LoadURL(CefString(url));
  if (tab->view) {
    tab->view->RequestFocus();
  }
  BroadcastState();
  return true;
}

bool BrowserWindow::GoBack() {
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser || !browser->CanGoBack()) {
    return false;
  }
  browser->GoBack();
  return true;
}

bool BrowserWindow::GoForward() {
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser || !browser->CanGoForward()) {
    return false;
  }
  browser->GoForward();
  return true;
}

bool BrowserWindow::Reload() {
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return false;
  }
  browser->Reload();
  return true;
}

bool BrowserWindow::Stop() {
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return false;
  }
  browser->StopLoad();
  return true;
}

void BrowserWindow::SetActivePanel(const std::string& panel) {
  CEF_REQUIRE_UI_THREAD();
  active_panel_ = panel;
  SetDashboardOpen(panel != "none");
}

double BrowserWindow::ZoomIn() {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = ActiveTab();
  if (!tab || !tab->browser) return 0.0;
  tab->zoom_level += 0.2;
  if (tab->zoom_level > 3.0) tab->zoom_level = 3.0;
  tab->browser->GetHost()->SetZoomLevel(tab->zoom_level);
  BroadcastState();
  return tab->zoom_level;
}

double BrowserWindow::ZoomOut() {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = ActiveTab();
  if (!tab || !tab->browser) return 0.0;
  tab->zoom_level -= 0.2;
  if (tab->zoom_level < -2.0) tab->zoom_level = -2.0;
  tab->browser->GetHost()->SetZoomLevel(tab->zoom_level);
  BroadcastState();
  return tab->zoom_level;
}

double BrowserWindow::ResetZoom() {
  CEF_REQUIRE_UI_THREAD();
  Tab* tab = ActiveTab();
  if (!tab || !tab->browser) return 0.0;
  tab->zoom_level = 0.0;
  tab->browser->GetHost()->SetZoomLevel(0.0);
  BroadcastState();
  return 0.0;
}

void BrowserWindow::FindInPage(const std::string& query, bool forward, bool find_next) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser || query.empty()) return;
  browser->GetHost()->Find(CefString(query), forward, false, find_next);
}

void BrowserWindow::StopFinding(bool clear_selection) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) return;
  browser->GetHost()->StopFinding(clear_selection);
}

void BrowserWindow::OnDownloadStarted(const db::DownloadRecord& record, CefRefPtr<CefDownloadItemCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  if (db_) {
    db_->UpsertDownload(record);
  }
  if (callback) {
    active_downloads_[record.guid] = callback;
  }
  BroadcastDownloads();
}

void BrowserWindow::OnDownloadUpdated(CefRefPtr<CefDownloadItem> item, CefRefPtr<CefDownloadItemCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  if (!item || !db_) return;

  const std::string guid = std::to_string(item->GetId());
  if (callback) {
    active_downloads_[guid] = callback;
  }

  db::DownloadRecord rec;
  rec.guid = guid;
  rec.url = item->GetURL().ToString();
  rec.file_path = item->GetFullPath().ToString();
  rec.file_name = std::filesystem::u8path(rec.file_path).filename().string();
  rec.total_bytes = item->GetTotalBytes();
  rec.received_bytes = item->GetReceivedBytes();

  if (item->IsComplete()) {
    rec.state = "completed";
    rec.completed_at = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    active_downloads_.erase(guid);
  } else if (item->IsCanceled()) {
    rec.state = "cancelled";
    active_downloads_.erase(guid);
  } else {
    rec.state = "in_progress";
  }

  db_->UpsertDownload(rec);
  BroadcastDownloads();
}

bool BrowserWindow::CancelDownload(const std::string& guid) {
  CEF_REQUIRE_UI_THREAD();
  auto it = active_downloads_.find(guid);
  if (it != active_downloads_.end() && it->second) {
    it->second->Cancel();
    active_downloads_.erase(it);
  }
  if (db_) {
    db::DownloadRecord rec;
    rec.guid = guid;
    rec.state = "cancelled";
    db_->UpsertDownload(rec);
  }
  BroadcastDownloads();
  return true;
}

bool BrowserWindow::OpenDownloadedFile(const std::string& guid) {
  if (!db_) return false;
  auto list = db_->GetDownloads(50);
  for (const auto& item : list) {
    if (item.guid == guid && !item.file_path.empty()) {
      std::wstring wide = std::filesystem::u8path(item.file_path).wstring();
      ::ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      return true;
    }
  }
  return false;
}

bool BrowserWindow::OpenDownloadedFolder(const std::string& guid) {
  if (!db_) return false;
  auto list = db_->GetDownloads(50);
  for (const auto& item : list) {
    if (item.guid == guid && !item.file_path.empty()) {
      std::wstring folder = std::filesystem::u8path(item.file_path).parent_path().wstring();
      ::ShellExecuteW(nullptr, L"explore", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      return true;
    }
  }
  return false;
}

void BrowserWindow::BroadcastDownloads() {
  if (!db_) return;
  auto list = db_->GetDownloads(50);
  CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
  CefRefPtr<CefListValue> arr = CefListValue::Create();
  for (size_t i = 0; i < list.size(); ++i) {
    const auto& item = list[i];
    CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
    dict->SetInt("id", static_cast<int>(item.id));
    dict->SetString("guid", CefString(item.guid));
    dict->SetString("url", CefString(item.url));
    dict->SetString("fileName", CefString(item.file_name));
    dict->SetString("filePath", CefString(item.file_path));
    dict->SetDouble("totalBytes", static_cast<double>(item.total_bytes));
    dict->SetDouble("receivedBytes", static_cast<double>(item.received_bytes));
    dict->SetDouble("speed", 0.0);
    double pct = item.total_bytes > 0 ? (100.0 * item.received_bytes / item.total_bytes) : 0.0;
    dict->SetDouble("percent", pct);
    dict->SetString("state", CefString(item.state));
    dict->SetDouble("createdAt", static_cast<double>(item.created_at));
    dict->SetDouble("completedAt", static_cast<double>(item.completed_at));
    arr->SetDictionary(i, dict);
  }
  payload->SetList("items", arr);
  SendEvent("downloads.changed", payload);
}

void BrowserWindow::OnMediaDetected(const db::MediaItem& item) {
  CEF_REQUIRE_UI_THREAD();
  if (db_) {
    db_->AddMediaItem(item);
  }
  CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
  payload->SetInt("id", static_cast<int>(item.id));
  payload->SetString("url", CefString(item.url));
  payload->SetString("sourceUrl", CefString(item.source_url));
  payload->SetString("mediaType", CefString(item.media_type));
  payload->SetString("title", CefString(item.title));
  payload->SetDouble("sizeBytes", static_cast<double>(item.size_bytes));
  payload->SetDouble("discoveredAt", static_cast<double>(item.discovered_at));
  SendEvent("media.detected", payload);
}

void BrowserWindow::SetDashboardOpen(bool open) {
  CEF_REQUIRE_UI_THREAD();
  if (dashboard_open_ == open) {
    return;
  }
  dashboard_open_ = open;

  LayoutViews();
  UpdateVisibility();

  CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
  payload->SetBool("open", open);
  SendEvent("ui.dashboardChanged", payload);
  BroadcastState();

  if (open && shell_view_) {
    shell_view_->RequestFocus();
  }
}

void BrowserWindow::FocusAddressBar() {
  CEF_REQUIRE_UI_THREAD();
  std::fprintf(stderr, "NEXUS FocusAddressBar called\n");
  std::fflush(stderr);
  if (window_) {
    window_->Activate();
  }
  if (shell_view_) {
    shell_view_->RequestFocus();
  }
  SendEvent("ui.focusAddress", CefDictionaryValue::Create());
}

void BrowserWindow::SetCookieMode(CookieMode mode) {
  CEF_REQUIRE_UI_THREAD();
  if (privacy_->SetCookieMode(mode)) {
    BroadcastPrivacyState();
  }
}

bool BrowserWindow::StartClearCookies() {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(nullptr);
  if (!manager) {
    return false;
  }
  return manager->DeleteCookies(
      CefString(), CefString(),
      new ClearCookiesCallback(this, privacy_));
}

void BrowserWindow::OnCookiesCleared() {
  CEF_REQUIRE_UI_THREAD();
  BroadcastPrivacyState();
}

BrowserWindow::Tab* BrowserWindow::FindTab(int tab_id) {
  for (Tab& tab : tabs_) {
    if (tab.id == tab_id) {
      return &tab;
    }
  }
  return nullptr;
}

const BrowserWindow::Tab* BrowserWindow::FindTab(int tab_id) const {
  for (const Tab& tab : tabs_) {
    if (tab.id == tab_id) {
      return &tab;
    }
  }
  return nullptr;
}

BrowserWindow::Tab* BrowserWindow::ActiveTab() {
  return FindTab(active_tab_id_);
}

CefRefPtr<CefBrowser> BrowserWindow::ActiveBrowser() {
  Tab* tab = ActiveTab();
  return tab ? tab->browser : nullptr;
}

void BrowserWindow::LayoutViews() {
  if (!window_ || !shell_view_) {
    return;
  }

  const CefRect client = window_->GetClientAreaBoundsInScreen();
  const int width = std::max(1, client.width);
  const int height = std::max(1, client.height);
  std::fprintf(stderr, "NEXUS LayoutViews: client=%d,%d %dx%d shell=%d tabs=%zu dashboard=%d\\n",
               client.x, client.y, client.width, client.height, kShellHeight,
               tabs_.size(), dashboard_open_ ? 1 : 0);
  std::fflush(stderr);

  if (dashboard_open_) {
    shell_view_->SetBounds(CefRect(0, 0, width, height));
    if (window_) {
      window_->Layout();
    }
    return;
  }

  const int shell_height = std::min(kShellHeight, height);
  shell_view_->SetBounds(CefRect(0, 0, width, shell_height));

  const int content_height = std::max(0, height - shell_height);
  for (Tab& tab : tabs_) {
    if (tab.view) {
      tab.view->SetBounds(CefRect(0, shell_height, width, content_height));
    }
  }
  if (window_) {
    window_->Layout();
  }
}

void BrowserWindow::UpdateVisibility() {
  if (shell_view_) {
    shell_view_->SetVisible(true);
  }
  for (Tab& tab : tabs_) {
    if (tab.view) {
      tab.view->SetVisible(!dashboard_open_ && tab.id == active_tab_id_);
    }
  }
}

void BrowserWindow::UpdateWindowTitle() {
  if (!window_) {
    return;
  }
  const Tab* tab = FindTab(active_tab_id_);
  std::string title = tab ? tab->title : std::string();
  if (title.empty()) {
    title = "NEXUS";
  } else {
    title += " - NEXUS";
  }
  window_->SetTitle(CefString(title));
}

void BrowserWindow::BroadcastState() {
  if (!shell_loaded_ || !shell_browser_) {
    return;
  }
  SendEvent("browser.stateChanged", BuildBrowserState());
}

void BrowserWindow::BroadcastPrivacyState() {
  if (!shell_loaded_ || !shell_browser_) {
    return;
  }
  SendEvent("privacy.stateChanged", BuildPrivacyState());
}

void BrowserWindow::SendEvent(const std::string& name,
                              CefRefPtr<CefDictionaryValue> payload) {
  if (!payload) {
    payload = CefDictionaryValue::Create();
  }

  CefRefPtr<CefDictionaryValue> event = CefDictionaryValue::Create();
  event->SetInt("v", 1);
  event->SetString("type", "event");
  event->SetString("event", CefString(name));
  event->SetDictionary("payload", payload);

  CefRefPtr<CefValue> root = CefValue::Create();
  root->SetDictionary(event);
  SendIpcJson(CefWriteJSON(root, JSON_WRITER_DEFAULT).ToString());
}

void BrowserWindow::ApplyDarkTitleBar() {
  if (!window_) {
    return;
  }
  HWND hwnd = window_->GetWindowHandle();
  if (!hwnd) {
    return;
  }

  BOOL dark = TRUE;
  ::DwmSetWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &dark,
                          sizeof(dark));
  const COLORREF caption = RGB(0x11, 0x13, 0x15);
  ::DwmSetWindowAttribute(hwnd, kDwmwaCaptionColor, &caption,
                          sizeof(caption));
  const COLORREF text = RGB(0xF1, 0xF3, 0xF5);
  ::DwmSetWindowAttribute(hwnd, kDwmwaTextColor, &text, sizeof(text));
}

}  // namespace nexus
