#include "src/ipc_router.h"

#include <string>

#include "core/url_utils.h"
#include "src/browser_window.h"
#include "src/privacy_service.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"

namespace nexus {
namespace {

constexpr size_t kMaxRequestBytes = 64 * 1024;
constexpr int kRateLimitCount = 240;
constexpr int kRateLimitSeconds = 5;

CefRefPtr<CefValue> DictionaryToValue(
    CefRefPtr<CefDictionaryValue> dictionary) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetDictionary(dictionary);
  return value;
}

CefRefPtr<CefDictionaryValue> EmptyDictionary() {
  return CefDictionaryValue::Create();
}

bool ReadString(CefRefPtr<CefDictionaryValue> dictionary,
                const char* key,
                std::string* output,
                bool required,
                std::string* error) {
  CefRefPtr<CefValue> value = dictionary->GetValue(key);
  if (!value || value->GetType() == VTYPE_NULL) {
    if (required) {
      *error = std::string("Missing required field: ") + key;
      return false;
    }
    return true;
  }
  if (value->GetType() != VTYPE_STRING) {
    *error = std::string("Field must be a string: ") + key;
    return false;
  }
  *output = value->GetString().ToString();
  return true;
}

bool ReadInt(CefRefPtr<CefDictionaryValue> dictionary,
             const char* key,
             int* output,
             bool required,
             std::string* error) {
  CefRefPtr<CefValue> value = dictionary->GetValue(key);
  if (!value || value->GetType() == VTYPE_NULL) {
    if (required) {
      *error = std::string("Missing required field: ") + key;
      return false;
    }
    return true;
  }
  if (value->GetType() != VTYPE_INT) {
    *error = std::string("Field must be an integer: ") + key;
    return false;
  }
  *output = value->GetInt();
  return true;
}

bool ReadBool(CefRefPtr<CefDictionaryValue> dictionary,
              const char* key,
              bool* output,
              bool required,
              std::string* error) {
  CefRefPtr<CefValue> value = dictionary->GetValue(key);
  if (!value || value->GetType() == VTYPE_NULL) {
    if (required) {
      *error = std::string("Missing required field: ") + key;
      return false;
    }
    return true;
  }
  if (value->GetType() != VTYPE_BOOL) {
    *error = std::string("Field must be a boolean: ") + key;
    return false;
  }
  *output = value->GetBool();
  return true;
}

bool IsKnownMethod(const std::string& method) {
  return method == "app.getInfo" || method == "browser.getState" ||
         method == "browser.createTab" || method == "browser.closeTab" ||
         method == "browser.duplicateTab" || method == "browser.restoreTab" ||
         method == "browser.activateTab" || method == "browser.navigate" ||
         method == "browser.goBack" || method == "browser.goForward" ||
         method == "browser.reload" || method == "browser.stop" ||
         method == "browser.zoomIn" || method == "browser.zoomOut" ||
         method == "browser.resetZoom" || method == "browser.find" ||
         method == "browser.stopFinding" ||
         method == "ui.setDashboardOpen" || method == "ui.setActivePanel" ||
         method == "ui.focusAddress" ||
         method == "privacy.getState" || method == "privacy.setCookieMode" ||
         method == "privacy.clearCookies" ||
         method == "history.get" || method == "history.autocomplete" ||
         method == "history.delete" || method == "history.clear" ||
         method == "bookmarks.get" || method == "bookmarks.add" ||
         method == "bookmarks.remove" || method == "bookmarks.isBookmarked" ||
         method == "downloads.get" || method == "downloads.cancel" ||
         method == "downloads.openFile" || method == "downloads.openFolder" ||
         method == "media.get" || method == "media.download" || method == "media.clear" ||
         method == "vpn.getStatus" || method == "vpn.getConfigs" ||
         method == "vpn.importConfig" || method == "vpn.connect" ||
         method == "vpn.disconnect" || method == "vpn.deleteConfig" ||
         method == "vpn.setKillSwitch" ||
         method == "permissions.getAll" || method == "permissions.set" ||
         method == "permissions.clear" ||
         method == "settings.get" || method == "settings.set";
}

}  // namespace

IpcRouter::IpcRouter(BrowserWindow* window)
    : window_(window), rate_window_start_(std::chrono::steady_clock::now()) {}

bool IpcRouter::HandleMessage(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefProcessId source_process,
                              CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();

  if (source_process != PID_RENDERER || !browser || !frame || !message) {
    return false;
  }
  if (message->GetName() != "nexus.ipc.message" || !frame->IsMain()) {
    return false;
  }
  if (!window_->IsShellBrowser(browser) ||
      !core::IsInternalAppUrl(frame->GetURL().ToString())) {
    return false;
  }

  CefRefPtr<CefListValue> arguments = message->GetArgumentList();
  if (!arguments || arguments->GetSize() != 1) {
    return true;
  }
  CefRefPtr<CefValue> argument = arguments->GetValue(0);
  if (!argument || argument->GetType() != VTYPE_STRING) {
    return true;
  }

  const std::string raw = argument->GetString().ToString();
  if (raw.empty() || raw.size() > kMaxRequestBytes) {
    SendError("", "invalid_request", "IPC request is empty or too large.");
    return true;
  }

  CefRefPtr<CefValue> parsed = CefParseJSON(CefString(raw), JSON_PARSER_RFC);
  if (!parsed || parsed->GetType() != VTYPE_DICTIONARY) {
    SendError("", "invalid_request", "IPC request must be a JSON object.");
    return true;
  }

  CefRefPtr<CefDictionaryValue> request = parsed->GetDictionary();
  std::string id;
  std::string error;
  CefRefPtr<CefValue> id_value = request->GetValue("id");
  if (id_value) {
    if (id_value->GetType() == VTYPE_STRING) {
      id = id_value->GetString().ToString();
    } else if (id_value->GetType() == VTYPE_INT) {
      id = std::to_string(id_value->GetInt());
    }
  }
  if (id.size() > 128) {
    id.clear();
  }

  CefRefPtr<CefValue> version_value = request->GetValue("v");
  if (!version_value || version_value->GetType() != VTYPE_INT ||
      version_value->GetInt() != 1) {
    SendError(id, "invalid_request", "Unsupported IPC protocol version.");
    return true;
  }

  std::string method;
  if (!ReadString(request, "method", &method, true, &error)) {
    SendError(id, "invalid_request", error);
    return true;
  }
  if (method.size() > 64 || !IsKnownMethod(method)) {
    SendError(id, "unknown_method", "Unknown IPC method.");
    return true;
  }

  CefRefPtr<CefDictionaryValue> payload = EmptyDictionary();
  CefRefPtr<CefValue> payload_value = request->GetValue("payload");
  if (payload_value && payload_value->GetType() != VTYPE_NULL) {
    if (payload_value->GetType() != VTYPE_DICTIONARY) {
      SendError(id, "invalid_request", "IPC payload must be an object.");
      return true;
    }
    payload = payload_value->GetDictionary();
  }

  const auto now = std::chrono::steady_clock::now();
  if (now - rate_window_start_ >
      std::chrono::seconds(kRateLimitSeconds)) {
    rate_window_start_ = now;
    rate_window_count_ = 0;
  }
  if (++rate_window_count_ > kRateLimitCount) {
    SendError(id, "rate_limited", "Too many IPC requests.");
    return true;
  }

  CefRefPtr<CefValue> result;
  std::string dispatch_error_code;
  std::string dispatch_error_message;
  if (!Dispatch(method, payload, &result, &dispatch_error_code,
                &dispatch_error_message)) {
    SendError(id, dispatch_error_code, dispatch_error_message);
    return true;
  }

  SendResponse(id, true, result, "", "");
  return true;
}

bool IpcRouter::Dispatch(const std::string& method,
                         CefRefPtr<CefDictionaryValue> payload,
                         CefRefPtr<CefValue>* result,
                         std::string* error_code,
                         std::string* error_message) {
  std::string field_error;

  if (method == "app.getInfo") {
    *result = DictionaryToValue(window_->BuildAppInfo());
    return true;
  }
  if (method == "browser.getState") {
    *result = DictionaryToValue(window_->BuildBrowserState());
    return true;
  }
  if (method == "privacy.getState") {
    *result = DictionaryToValue(window_->BuildPrivacyState());
    return true;
  }
  if (method == "ui.focusAddress") {
    window_->FocusAddressBar();
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "browser.createTab") {
    std::string url;
    bool activate = true;
    if (!ReadString(payload, "url", &url, false, &field_error) ||
        !ReadBool(payload, "activate", &activate, false, &field_error)) {
      *error_code = "invalid_request";
      *error_message = field_error;
      return false;
    }

    std::string target = "nexus://newtab/";
    bool search = false;
    if (!url.empty()) {
      const core::NavigationTarget resolved =
          core::ResolveNavigationInput(url);
      if (!resolved.allowed) {
        *error_code = "invalid_url";
        *error_message = resolved.error;
        return false;
      }
      target = resolved.url;
      search = resolved.search;
    }

    const int tab_id = window_->CreateTab(target, activate);
    CefRefPtr<CefDictionaryValue> response = EmptyDictionary();
    response->SetInt("tabId", tab_id);
    response->SetString("url", CefString(target));
    response->SetBool("search", search);
    *result = DictionaryToValue(response);
    return true;
  }
  if (method == "browser.closeTab") {
    int tab_id = 0;
    if (!ReadInt(payload, "tabId", &tab_id, false, &field_error)) {
      *error_code = "invalid_request";
      *error_message = field_error;
      return false;
    }
    if (!window_->CloseTab(tab_id)) {
      *error_code = "unknown_tab";
      *error_message = "No such tab.";
      return false;
    }
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "browser.activateTab") {
    int tab_id = 0;
    if (!ReadInt(payload, "tabId", &tab_id, true, &field_error)) {
      *error_code = "invalid_request";
      *error_message = field_error;
      return false;
    }
    if (!window_->ActivateTab(tab_id)) {
      *error_code = "unknown_tab";
      *error_message = "No such tab.";
      return false;
    }
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "browser.navigate") {
    std::string input;
    if (!ReadString(payload, "url", &input, true, &field_error)) {
      *error_code = "invalid_request";
      *error_message = field_error;
      return false;
    }
    const core::NavigationTarget resolved =
        core::ResolveNavigationInput(input);
    if (!resolved.allowed) {
      *error_code = "invalid_url";
      *error_message = resolved.error;
      return false;
    }
    if (!window_->NavigateActive(resolved.url)) {
      *error_code = "no_active_tab";
      *error_message = "There is no active tab.";
      return false;
    }
    CefRefPtr<CefDictionaryValue> response = EmptyDictionary();
    response->SetString("url", CefString(resolved.url));
    response->SetBool("search", resolved.search);
    *result = DictionaryToValue(response);
    return true;
  }
  if (method == "browser.goBack" || method == "browser.goForward" ||
      method == "browser.reload" || method == "browser.stop") {
    bool handled = false;
    if (method == "browser.goBack") {
      handled = window_->GoBack();
    } else if (method == "browser.goForward") {
      handled = window_->GoForward();
    } else if (method == "browser.reload") {
      handled = window_->Reload();
    } else {
      handled = window_->Stop();
    }
    if (!handled) {
      *error_code = "no_active_tab";
      *error_message = "There is no active tab.";
      return false;
    }
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "ui.setDashboardOpen") {
    bool open = false;
    if (!ReadBool(payload, "open", &open, true, &field_error)) {
      *error_code = "invalid_request";
      *error_message = field_error;
      return false;
    }
    window_->SetDashboardOpen(open);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "privacy.setCookieMode") {
    std::string mode;
    if (!ReadString(payload, "mode", &mode, true, &field_error)) {
      *error_code = "invalid_request";
      *error_message = field_error;
      return false;
    }
    if (mode != "strict" && mode != "balanced") {
      *error_code = "invalid_request";
      *error_message = "Cookie mode must be strict or balanced.";
      return false;
    }
    window_->SetCookieMode(PrivacyService::CookieModeFromString(mode));
    *result = DictionaryToValue(window_->BuildPrivacyState());
    return true;
  }
  if (method == "privacy.clearCookies") {
    if (!window_->StartClearCookies()) {
      *error_code = "cookies_unavailable";
      *error_message = "The cookie store is not available.";
      return false;
    }
    CefRefPtr<CefDictionaryValue> response = EmptyDictionary();
    response->SetBool("started", true);
    *result = DictionaryToValue(response);
    return true;
  }

  // --- Browser Extra & Zoom & Find ---
  if (method == "browser.duplicateTab") {
    int tab_id = 0;
    ReadInt(payload, "tabId", &tab_id, false, &field_error);
    int new_id = window_->DuplicateTab(tab_id);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetInt("tabId", new_id);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "browser.restoreTab") {
    bool ok = window_->RestoreTab();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetBool("restored", ok);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "browser.zoomIn") {
    double z = window_->ZoomIn();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetDouble("zoomLevel", z);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "browser.zoomOut") {
    double z = window_->ZoomOut();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetDouble("zoomLevel", z);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "browser.resetZoom") {
    double z = window_->ResetZoom();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetDouble("zoomLevel", z);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "browser.find") {
    std::string query;
    ReadString(payload, "query", &query, true, &field_error);
    bool forward = true;
    ReadBool(payload, "forward", &forward, false, &field_error);
    bool findNext = false;
    ReadBool(payload, "findNext", &findNext, false, &field_error);
    window_->FindInPage(query, forward, findNext);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "browser.stopFinding") {
    bool clear_selection = false;
    ReadBool(payload, "clearSelection", &clear_selection, false, &field_error);
    window_->StopFinding(clear_selection);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- UI Panels ---
  if (method == "ui.setActivePanel") {
    std::string panel;
    ReadString(payload, "panel", &panel, true, &field_error);
    window_->SetActivePanel(panel);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- History ---
  if (method == "history.get") {
    int limit = 100;
    ReadInt(payload, "limit", &limit, false, &field_error);
    std::string query;
    ReadString(payload, "query", &query, false, &field_error);
    auto items = window_->database()->GetHistory(limit, query);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < items.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetInt("id", static_cast<int>(items[i].id));
      d->SetString("url", CefString(items[i].url));
      d->SetString("title", CefString(items[i].title));
      d->SetDouble("visitTime", static_cast<double>(items[i].visit_time));
      d->SetInt("visitCount", items[i].visit_count);
      list->SetDictionary(i, d);
    }
    resp->SetList("items", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "history.autocomplete") {
    std::string prefix;
    ReadString(payload, "prefix", &prefix, true, &field_error);
    int limit = 8;
    ReadInt(payload, "limit", &limit, false, &field_error);
    auto items = window_->database()->SearchAutocomplete(prefix, limit);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < items.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetInt("id", static_cast<int>(items[i].id));
      d->SetString("url", CefString(items[i].url));
      d->SetString("title", CefString(items[i].title));
      d->SetDouble("visitTime", static_cast<double>(items[i].visit_time));
      d->SetInt("visitCount", items[i].visit_count);
      list->SetDictionary(i, d);
    }
    resp->SetList("items", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "history.delete") {
    int id = 0;
    ReadInt(payload, "id", &id, true, &field_error);
    window_->database()->DeleteHistoryItem(id);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "history.clear") {
    int older_than = 0;
    ReadInt(payload, "olderThanMs", &older_than, false, &field_error);
    window_->database()->ClearHistory(older_than);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- Bookmarks ---
  if (method == "bookmarks.get") {
    auto items = window_->database()->GetBookmarks();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < items.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetInt("id", static_cast<int>(items[i].id));
      d->SetString("url", CefString(items[i].url));
      d->SetString("title", CefString(items[i].title));
      d->SetDouble("createdAt", static_cast<double>(items[i].created_at));
      list->SetDictionary(i, d);
    }
    resp->SetList("items", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "bookmarks.add") {
    std::string url, title;
    ReadString(payload, "url", &url, true, &field_error);
    ReadString(payload, "title", &title, false, &field_error);
    window_->database()->AddBookmark(url, title.empty() ? url : title);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetString("url", CefString(url));
    resp->SetString("title", CefString(title));
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "bookmarks.remove") {
    std::string url;
    ReadString(payload, "url", &url, true, &field_error);
    window_->database()->RemoveBookmark(url);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "bookmarks.isBookmarked") {
    std::string url;
    ReadString(payload, "url", &url, true, &field_error);
    bool bm = window_->database()->IsBookmarked(url);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetBool("bookmarked", bm);
    *result = DictionaryToValue(resp);
    return true;
  }

  // --- Downloads ---
  if (method == "downloads.get") {
    int limit = 50;
    ReadInt(payload, "limit", &limit, false, &field_error);
    auto items = window_->database()->GetDownloads(limit);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < items.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetInt("id", static_cast<int>(items[i].id));
      d->SetString("guid", CefString(items[i].guid));
      d->SetString("url", CefString(items[i].url));
      d->SetString("fileName", CefString(items[i].file_name));
      d->SetString("filePath", CefString(items[i].file_path));
      d->SetDouble("totalBytes", static_cast<double>(items[i].total_bytes));
      d->SetDouble("receivedBytes", static_cast<double>(items[i].received_bytes));
      d->SetDouble("speed", 0.0);
      double pct = items[i].total_bytes > 0 ? (100.0 * items[i].received_bytes / items[i].total_bytes) : 0.0;
      d->SetDouble("percent", pct);
      d->SetString("state", CefString(items[i].state));
      d->SetDouble("createdAt", static_cast<double>(items[i].created_at));
      d->SetDouble("completedAt", static_cast<double>(items[i].completed_at));
      list->SetDictionary(i, d);
    }
    resp->SetList("items", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "downloads.cancel") {
    std::string guid;
    ReadString(payload, "guid", &guid, true, &field_error);
    window_->CancelDownload(guid);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "downloads.openFile") {
    std::string guid;
    ReadString(payload, "guid", &guid, true, &field_error);
    window_->OpenDownloadedFile(guid);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "downloads.openFolder") {
    std::string guid;
    ReadString(payload, "guid", &guid, true, &field_error);
    window_->OpenDownloadedFolder(guid);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- Media ---
  if (method == "media.get") {
    std::string source_url;
    ReadString(payload, "sourceUrl", &source_url, false, &field_error);
    int limit = 50;
    ReadInt(payload, "limit", &limit, false, &field_error);
    auto items = window_->database()->GetMediaItems(source_url, limit);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < items.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetInt("id", static_cast<int>(items[i].id));
      d->SetString("url", CefString(items[i].url));
      d->SetString("sourceUrl", CefString(items[i].source_url));
      d->SetString("mediaType", CefString(items[i].media_type));
      d->SetString("title", CefString(items[i].title));
      d->SetDouble("sizeBytes", static_cast<double>(items[i].size_bytes));
      d->SetDouble("discoveredAt", static_cast<double>(items[i].discovered_at));
      list->SetDictionary(i, d);
    }
    resp->SetList("items", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "media.download") {
    std::string url;
    ReadString(payload, "url", &url, true, &field_error);
    window_->CreateTab(url, false);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetBool("started", true);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "media.clear") {
    window_->database()->ClearMediaItems();
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- VPN ---
  if (method == "vpn.getStatus") {
    auto status = window_->vpn()->GetStatus();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    std::string st = "disconnected";
    if (status.status == vpn::VpnStatus::kConnected) st = "connected";
    else if (status.status == vpn::VpnStatus::kConnecting) st = "connecting";
    else if (status.status == vpn::VpnStatus::kError) st = "error";
    resp->SetString("status", CefString(st));
    resp->SetString("configName", CefString(status.config_name));
    resp->SetString("endpoint", CefString(status.endpoint));
    resp->SetString("interfaceIp", CefString(status.interface_ip));
    resp->SetString("dns", CefString(status.dns));
    resp->SetDouble("uptimeSeconds", static_cast<double>(status.uptime_seconds));
    resp->SetDouble("bytesSent", static_cast<double>(status.bytes_sent));
    resp->SetDouble("bytesReceived", static_cast<double>(status.bytes_received));
    resp->SetBool("killSwitchEnabled", status.kill_switch_enabled);
    resp->SetBool("dnsLeakProtection", status.dns_leak_protection);
    resp->SetString("errorMessage", CefString(status.error_message));
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "vpn.getConfigs") {
    auto configs = window_->database()->GetVpnConfigs();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < configs.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetInt("id", static_cast<int>(configs[i].id));
      d->SetString("name", CefString(configs[i].name));
      d->SetString("content", CefString(configs[i].config_content));
      d->SetString("interfaceAddress", CefString(configs[i].interface_address));
      d->SetString("endpoint", CefString(configs[i].endpoint));
      d->SetDouble("createdAt", static_cast<double>(configs[i].created_at));
      d->SetBool("isActive", configs[i].is_active);
      list->SetDictionary(i, d);
    }
    resp->SetList("configs", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "vpn.importConfig") {
    std::string name, content;
    ReadString(payload, "name", &name, true, &field_error);
    ReadString(payload, "content", &content, true, &field_error);
    std::string iface, endpoint, dns;
    vpn::VpnManager::ParseConfig(content, &iface, &endpoint, &dns);
    bool ok = window_->database()->AddVpnConfig(name, content, iface, endpoint);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetBool("success", ok);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "vpn.connect") {
    std::string name, content;
    ReadString(payload, "name", &name, true, &field_error);
    ReadString(payload, "content", &content, false, &field_error);
    if (content.empty()) {
      auto configs = window_->database()->GetVpnConfigs();
      for (const auto& c : configs) {
        if (c.name == name) {
          content = c.config_content;
          window_->database()->SetActiveVpnConfig(c.id);
          break;
        }
      }
    }
    window_->vpn()->Connect(name, content);
    auto status = window_->vpn()->GetStatus();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetString("status", "connected");
    resp->SetString("configName", CefString(status.config_name));
    resp->SetString("endpoint", CefString(status.endpoint));
    resp->SetString("interfaceIp", CefString(status.interface_ip));
    resp->SetString("dns", CefString(status.dns));
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "vpn.disconnect") {
    window_->vpn()->Disconnect();
    window_->database()->SetActiveVpnConfig(0);
    auto status = window_->vpn()->GetStatus();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetString("status", "disconnected");
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "vpn.deleteConfig") {
    int id = 0;
    ReadInt(payload, "id", &id, true, &field_error);
    window_->database()->DeleteVpnConfig(id);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "vpn.setKillSwitch") {
    bool enabled = true;
    ReadBool(payload, "enabled", &enabled, true, &field_error);
    window_->vpn()->SetKillSwitch(enabled);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- Site Permissions ---
  if (method == "permissions.getAll") {
    auto perms = window_->database()->GetSitePermissions();
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    CefRefPtr<CefListValue> list = CefListValue::Create();
    for (size_t i = 0; i < perms.size(); ++i) {
      CefRefPtr<CefDictionaryValue> d = EmptyDictionary();
      d->SetString("origin", CefString(perms[i].origin));
      d->SetString("permission", CefString(perms[i].permission));
      d->SetString("state", CefString(perms[i].state));
      d->SetDouble("updatedAt", static_cast<double>(perms[i].updated_at));
      list->SetDictionary(i, d);
    }
    resp->SetList("permissions", list);
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "permissions.set") {
    std::string origin, permission, state;
    ReadString(payload, "origin", &origin, true, &field_error);
    ReadString(payload, "permission", &permission, true, &field_error);
    ReadString(payload, "state", &state, true, &field_error);
    window_->database()->SetSitePermission(origin, permission, state);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }
  if (method == "permissions.clear") {
    std::string origin;
    ReadString(payload, "origin", &origin, false, &field_error);
    window_->database()->ClearSitePermissions(origin);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  // --- Settings ---
  if (method == "settings.get") {
    std::string key, default_val;
    ReadString(payload, "key", &key, true, &field_error);
    ReadString(payload, "defaultValue", &default_val, false, &field_error);
    std::string val = window_->database()->GetSetting(key, default_val);
    CefRefPtr<CefDictionaryValue> resp = EmptyDictionary();
    resp->SetString("value", CefString(val));
    *result = DictionaryToValue(resp);
    return true;
  }
  if (method == "settings.set") {
    std::string key, value;
    ReadString(payload, "key", &key, true, &field_error);
    ReadString(payload, "value", &value, true, &field_error);
    window_->database()->SetSetting(key, value);
    *result = DictionaryToValue(EmptyDictionary());
    return true;
  }

  *error_code = "unknown_method";
  *error_message = "Unknown IPC method.";
  return false;
}

void IpcRouter::SendResponse(const std::string& id,
                             bool ok,
                             CefRefPtr<CefValue> result,
                             const std::string& error_code,
                             const std::string& error_message) {
  CefRefPtr<CefDictionaryValue> response = EmptyDictionary();
  response->SetInt("v", 1);
  response->SetString("type", "response");
  response->SetString("id", CefString(id));
  response->SetBool("ok", ok);

  if (ok) {
    if (result) {
      response->SetValue("result", result);
    } else {
      response->SetDictionary("result", EmptyDictionary());
    }
  } else {
    CefRefPtr<CefDictionaryValue> error = EmptyDictionary();
    error->SetString("code", CefString(error_code));
    error->SetString("message", CefString(error_message));
    response->SetDictionary("error", error);
  }

  CefRefPtr<CefValue> root = CefValue::Create();
  root->SetDictionary(response);
  window_->SendIpcJson(CefWriteJSON(root, JSON_WRITER_DEFAULT).ToString());
}

void IpcRouter::SendError(const std::string& id,
                          const std::string& code,
                          const std::string& message) {
  SendResponse(id, false, nullptr, code, message);
}

}  // namespace nexus
