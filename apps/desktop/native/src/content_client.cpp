#include "src/content_client.h"

#include <windows.h>
#include <shlobj.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

#include "core/data_paths.h"
#include "core/url_utils.h"
#include "src/browser_window.h"
#include "include/cef_cookie.h"
#include "include/cef_resource_request_handler.h"
#include "include/cef_response.h"
#include "include/wrapper/cef_helpers.h"

namespace nexus {
namespace {

class NexusCookieFilter final : public CefCookieAccessFilter {
 public:
  explicit NexusCookieFilter(CefRefPtr<PrivacyService> privacy)
      : privacy_(std::move(privacy)) {}

  bool CanSendCookie(CefRefPtr<CefBrowser> /*browser*/,
                     CefRefPtr<CefFrame> /*frame*/,
                     CefRefPtr<CefRequest> request,
                     const CefCookie& cookie) override {
    return privacy_->ShouldAllowCookie(request, cookie);
  }

  bool CanSaveCookie(CefRefPtr<CefBrowser> /*browser*/,
                     CefRefPtr<CefFrame> /*frame*/,
                     CefRefPtr<CefRequest> request,
                     CefRefPtr<CefResponse> /*response*/,
                     const CefCookie& cookie) override {
    return privacy_->ShouldAllowCookie(request, cookie);
  }

 private:
  CefRefPtr<PrivacyService> privacy_;

  IMPLEMENT_REFCOUNTING(NexusCookieFilter);
  DISALLOW_COPY_AND_ASSIGN(NexusCookieFilter);
};

class NexusResourceRequestHandler final : public CefResourceRequestHandler {
 public:
  NexusResourceRequestHandler(CefRefPtr<PrivacyService> privacy,
                              CefRefPtr<BrowserWindow> window)
      : privacy_(std::move(privacy)), window_(std::move(window)) {}

  CefRefPtr<CefCookieAccessFilter> GetCookieAccessFilter(
      CefRefPtr<CefBrowser> /*browser*/,
      CefRefPtr<CefFrame> /*frame*/,
      CefRefPtr<CefRequest> /*request*/) override {
    return new NexusCookieFilter(privacy_);
  }

  ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser> /*browser*/,
                                   CefRefPtr<CefFrame> /*frame*/,
                                   CefRefPtr<CefRequest> request,
                                   CefRefPtr<CefCallback> /*callback*/) override {
    const std::string url = request->GetURL().ToString();
    const std::string initiator = request->GetHeaderByName("Origin").ToString();

    // Check tracker and advertisement blocker
    if (privacy_->ShouldBlockRequest(url, initiator)) {
      return RV_CANCEL;
    }

    const CefString referrer = request->GetReferrerURL();
    if (!referrer.empty()) {
      if (privacy_->mode() == CookieMode::kStrict) {
        request->SetReferrer(CefString(), REFERRER_POLICY_NO_REFERRER);
        privacy_->RecordStrippedReferrer();
      } else {
        // Same-origin requests keep their referrer; cross-origin requests are
        // reduced to the origin. This is stricter than Chromium's default.
        request->SetReferrer(
            referrer, REFERRER_POLICY_ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN);
      }
    }

    return RV_CONTINUE;
  }

  bool OnResourceResponse(CefRefPtr<CefBrowser> /*browser*/,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response) override {
    if (!window_) return false;
    const std::string mime = response->GetMimeType().ToString();
    const std::string url = request->GetURL().ToString();

    bool is_video = (mime.find("video/") != std::string::npos ||
                     url.find(".mp4") != std::string::npos ||
                     url.find(".webm") != std::string::npos);
    bool is_audio = (mime.find("audio/") != std::string::npos ||
                     url.find(".mp3") != std::string::npos ||
                     url.find(".m4a") != std::string::npos);
    bool is_stream = (url.find(".m3u8") != std::string::npos ||
                      url.find(".mpd") != std::string::npos ||
                      mime.find("mpegurl") != std::string::npos);

    if (is_video || is_audio || is_stream) {
      db::MediaItem item;
      item.url = url;
      item.source_url = frame ? frame->GetURL().ToString() : "";
      item.media_type = is_stream ? "stream" : (is_video ? "video" : "audio");
      item.title = core::HostOfUrl(url);
      window_->OnMediaDetected(item);
    }
    return false;
  }

 private:
  CefRefPtr<PrivacyService> privacy_;
  CefRefPtr<BrowserWindow> window_;

  IMPLEMENT_REFCOUNTING(NexusResourceRequestHandler);
  DISALLOW_COPY_AND_ASSIGN(NexusResourceRequestHandler);
};

bool IsHttpResource(const std::string& url) {
  return core::IsAllowedContentUrl(url) &&
         !core::IsInternalNewTabUrl(url);
}

}  // namespace

ContentClient::ContentClient(CefRefPtr<BrowserWindow> window,
                             int tab_id,
                             CefRefPtr<PrivacyService> privacy)
    : window_(std::move(window)),
      tab_id_(tab_id),
      privacy_(std::move(privacy)) {}

void ContentClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  window_->OnTabBrowserCreated(tab_id_, browser);
}

void ContentClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  window_->OnTabBrowserClosed(tab_id_);
  browser_ = nullptr;
}

bool ContentClient::OnBeforePopup(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    int /*popup_id*/,
    const CefString& /*target_url*/,
    const CefString& /*target_frame_name*/,
    cef_window_open_disposition_t /*target_disposition*/,
    bool /*user_gesture*/,
    const CefPopupFeatures& /*popup_features*/,
    CefWindowInfo& /*window_info*/,
    CefRefPtr<CefClient>& /*client*/,
    CefBrowserSettings& /*settings*/,
    CefRefPtr<CefDictionaryValue>& /*extra_info*/,
    bool* /*no_javascript_access*/) {
  CEF_REQUIRE_UI_THREAD();
  privacy_->RecordBlockedPopup();
  return true;
}

void ContentClient::OnAddressChange(CefRefPtr<CefBrowser> /*browser*/,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  CEF_REQUIRE_UI_THREAD();
  if (frame->IsMain()) {
    window_->OnTabUrl(tab_id_, url.ToString());
  }
}

void ContentClient::OnTitleChange(CefRefPtr<CefBrowser> /*browser*/,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  window_->OnTabTitle(tab_id_, title.ToString());
}

void ContentClient::OnLoadingStateChange(CefRefPtr<CefBrowser> /*browser*/,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  window_->OnTabLoadingState(tab_id_, isLoading, canGoBack, canGoForward);
}

void ContentClient::OnLoadError(CefRefPtr<CefBrowser> /*browser*/,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode error_code,
                                const CefString& error_text,
                                const CefString& failed_url) {
  CEF_REQUIRE_UI_THREAD();
  if (!frame->IsMain() || error_code == ERR_ABORTED) {
    return;
  }
  window_->OnTabLoadError(tab_id_, static_cast<int>(error_code),
                          error_text.ToString());
}

bool ContentClient::OnBeforeBrowse(CefRefPtr<CefBrowser> /*browser*/,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   bool /*user_gesture*/,
                                   bool /*is_redirect*/) {
  CEF_REQUIRE_UI_THREAD();
  if (!frame->IsMain()) {
    return false;
  }

  const std::string url = request->GetURL().ToString();
  if (core::IsAllowedContentUrl(url)) {
    return false;
  }

  privacy_->RecordBlockedNavigation();
  return true;
}

bool ContentClient::OnOpenURLFromTab(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    const CefString& target_url,
    cef_window_open_disposition_t target_disposition,
    bool /*user_gesture*/) {
  CEF_REQUIRE_UI_THREAD();
  if (target_disposition != CEF_WOD_NEW_FOREGROUND_TAB &&
      target_disposition != CEF_WOD_NEW_BACKGROUND_TAB) {
    return false;
  }

  const std::string url = target_url.ToString();
  if (!core::IsAllowedContentUrl(url)) {
    privacy_->RecordBlockedNavigation();
    return true;
  }

  window_->OpenUrlInNewTab(
      url, target_disposition == CEF_WOD_NEW_FOREGROUND_TAB);
  return true;
}

CefRefPtr<CefResourceRequestHandler>
ContentClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request,
    bool /*is_navigation*/,
    bool /*is_download*/,
    const CefString& /*request_initiator*/,
    bool& /*disable_default_handling*/) {
  if (IsHttpResource(request->GetURL().ToString())) {
    return new NexusResourceRequestHandler(privacy_, window_);
  }
  return nullptr;
}

bool ContentClient::OnCertificateError(CefRefPtr<CefBrowser> /*browser*/,
                                       cef_errorcode_t /*cert_error*/,
                                       const CefString& /*request_url*/,
                                       CefRefPtr<CefSSLInfo> /*ssl_info*/,
                                       CefRefPtr<CefCallback> /*callback*/) {
  // Never override Chromium's certificate validation. The default error page
  // is shown and the connection remains closed.
  return false;
}

void ContentClient::OnRenderProcessTerminated(
    CefRefPtr<CefBrowser> /*browser*/,
    TerminationStatus /*status*/,
    int /*error_code*/,
    const CefString& /*error_string*/) {
  CEF_REQUIRE_UI_THREAD();
  window_->OnTabRenderProcessTerminated(tab_id_);
}

bool ContentClient::OnRequestMediaAccessPermission(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    const CefString& /*requesting_origin*/,
    uint32_t /*requested_permissions*/,
    CefRefPtr<CefMediaAccessCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  privacy_->RecordDeniedPermission();
  callback->Cancel();
  return true;
}

bool ContentClient::OnShowPermissionPrompt(
    CefRefPtr<CefBrowser> /*browser*/,
    uint64_t /*prompt_id*/,
    const CefString& /*requesting_origin*/,
    uint32_t /*requested_permissions*/,
    CefRefPtr<CefPermissionPromptCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  privacy_->RecordDeniedPermission();
  callback->Continue(CEF_PERMISSION_RESULT_DENY);
  return true;
}

bool ContentClient::CanDownload(CefRefPtr<CefBrowser> /*browser*/,
                                const CefString& /*url*/,
                                const CefString& /*request_method*/) {
  CEF_REQUIRE_UI_THREAD();
  return true;
}

bool ContentClient::OnBeforeDownload(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  if (!download_item || !callback || !window_) {
    return false;
  }

  std::filesystem::path download_dir = window_->data_root() / L"downloads";
  PWSTR user_downloads = nullptr;
  if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &user_downloads))) {
    download_dir = user_downloads;
    ::CoTaskMemFree(user_downloads);
  }
  std::error_code ec;
  std::filesystem::create_directories(download_dir, ec);

  std::string raw_name = suggested_name.ToString();
  if (raw_name.empty()) {
    raw_name = "download.bin";
  }
  std::string safe_name;
  for (char c : raw_name) {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      safe_name.push_back('_');
    } else {
      safe_name.push_back(c);
    }
  }

  std::filesystem::path target_file = download_dir / std::filesystem::u8path(safe_name);
  int counter = 1;
  while (std::filesystem::exists(target_file, ec)) {
    auto stem = std::filesystem::u8path(safe_name).stem().u8string();
    auto ext = std::filesystem::u8path(safe_name).extension().u8string();
    std::string new_name = std::string(stem.begin(), stem.end()) + " (" +
                           std::to_string(counter++) + ")" +
                           std::string(ext.begin(), ext.end());
    target_file = download_dir / std::filesystem::u8path(new_name);
  }

  db::DownloadRecord record;
  record.guid = std::to_string(download_item->GetId());
  record.url = download_item->GetURL().ToString();
  record.file_name = target_file.filename().string();
  record.file_path = core::PathToUtf8(target_file);
  record.total_bytes = download_item->GetTotalBytes();
  record.received_bytes = download_item->GetReceivedBytes();
  record.state = "in_progress";
  record.created_at = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  window_->OnDownloadStarted(record, nullptr);
  callback->Continue(CefString(core::PathToUtf8(target_file)), false);
  return true;
}

void ContentClient::OnDownloadUpdated(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  CEF_REQUIRE_UI_THREAD();
  if (window_) {
    window_->OnDownloadUpdated(download_item, callback);
  }
}

}  // namespace nexus
