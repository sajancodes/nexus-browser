#include "src/content_client.h"

#include <string>
#include <utility>

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
  explicit NexusResourceRequestHandler(CefRefPtr<PrivacyService> privacy)
      : privacy_(std::move(privacy)) {}

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
    const CefString referrer = request->GetReferrerURL();
    if (referrer.empty()) {
      return RV_CONTINUE;
    }

    if (privacy_->mode() == CookieMode::kStrict) {
      request->SetReferrer(CefString(), REFERRER_POLICY_NO_REFERRER);
      privacy_->RecordStrippedReferrer();
    } else {
      // Same-origin requests keep their referrer; cross-origin requests are
      // reduced to the origin. This is stricter than Chromium's default.
      request->SetReferrer(
          referrer, REFERRER_POLICY_ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN);
    }
    return RV_CONTINUE;
  }

 private:
  CefRefPtr<PrivacyService> privacy_;

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
    return new NexusResourceRequestHandler(privacy_);
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
  privacy_->RecordBlockedDownload();
  return false;
}

bool ContentClient::OnBeforeDownload(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefDownloadItem> /*download_item*/,
    const CefString& /*suggested_name*/,
    CefRefPtr<CefBeforeDownloadCallback> /*callback*/) {
  CEF_REQUIRE_UI_THREAD();
  // Alloy style: returning false cancels the download. CanDownload() already
  // blocks user-initiated downloads; this is the defensive second check.
  return false;
}

}  // namespace nexus
