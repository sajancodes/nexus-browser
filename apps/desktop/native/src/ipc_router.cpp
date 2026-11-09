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
         method == "browser.activateTab" || method == "browser.navigate" ||
         method == "browser.goBack" || method == "browser.goForward" ||
         method == "browser.reload" || method == "browser.stop" ||
         method == "ui.setDashboardOpen" || method == "ui.focusAddress" ||
         method == "privacy.getState" || method == "privacy.setCookieMode" ||
         method == "privacy.clearCookies";
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
