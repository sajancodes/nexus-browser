#include "src/shell_client.h"

#include <cstdio>
#include <string>
#include <utility>

#include "core/url_utils.h"
#include "src/browser_window.h"
#include "include/wrapper/cef_helpers.h"

namespace nexus {

ShellClient::ShellClient(CefRefPtr<BrowserWindow> window)
    : window_(std::move(window)) {}

void ShellClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  window_->OnShellBrowserCreated(browser);
}

void ShellClient::OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = nullptr;
  window_->OnShellBrowserClosed();
}

void ShellClient::OnTitleChange(CefRefPtr<CefBrowser> /*browser*/,
                                const CefString& title) {
  std::fprintf(stderr, "NEXUS Shell title: %ls\n",
               title.ToWString().c_str());
  std::fflush(stderr);
  // The native window title is derived from the active content tab, never
  // from the shell document.
}

void ShellClient::OnLoadEnd(CefRefPtr<CefBrowser> /*browser*/,
                            CefRefPtr<CefFrame> frame,
                            int http_status_code) {
  CEF_REQUIRE_UI_THREAD();
  if (frame && frame->IsMain()) {
    std::fprintf(stderr, "NEXUS Shell OnLoadEnd url=%ls status=%d\n",
                 frame->GetURL().ToWString().c_str(), http_status_code);
    std::fflush(stderr);
    frame->ExecuteJavaScript(
        CefString("document.title = 'NEXUS_SHELL_' + "
                  "(document.body ? document.body.innerHTML.length : -1);"),
        frame->GetURL(), 0);
    window_->OnShellLoaded();
  }
}

void ShellClient::OnLoadError(CefRefPtr<CefBrowser> /*browser*/,
                              CefRefPtr<CefFrame> frame,
                              ErrorCode error_code,
                              const CefString& error_text,
                              const CefString& failed_url) {
  CEF_REQUIRE_UI_THREAD();
  if (!frame->IsMain() || error_code == ERR_ABORTED) {
    return;
  }
  std::fwprintf(stderr, L"NEXUS: shell load error %d (%ls) for %ls\n",
                static_cast<int>(error_code), error_text.ToWString().c_str(),
                failed_url.ToWString().c_str());
  std::fflush(stderr);
}

bool ShellClient::OnBeforeBrowse(CefRefPtr<CefBrowser> /*browser*/,
                                 CefRefPtr<CefFrame> /*frame*/,
                                 CefRefPtr<CefRequest> request,
                                 bool /*user_gesture*/,
                                 bool /*is_redirect*/) {
  CEF_REQUIRE_UI_THREAD();
  if (core::IsInternalAppUrl(request->GetURL().ToString())) {
    return false;
  }
  return true;
}

bool ShellClient::OnCertificateError(CefRefPtr<CefBrowser> /*browser*/,
                                     cef_errorcode_t /*cert_error*/,
                                     const CefString& /*request_url*/,
                                     CefRefPtr<CefSSLInfo> /*ssl_info*/,
                                     CefRefPtr<CefCallback> /*callback*/) {
  return false;
}

bool ShellClient::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();
  return window_->OnShellProcessMessage(browser, frame, source_process,
                                        message);
}

}  // namespace nexus
