#pragma once

#include <chrono>
#include <string>

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "include/cef_values.h"

namespace nexus {

class BrowserWindow;

// Validates and dispatches the small, fixed NEXUS IPC surface. Only messages
// from the shell browser's main frame (nexus://app) are accepted.
class IpcRouter {
 public:
  explicit IpcRouter(BrowserWindow* window);

  bool HandleMessage(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefProcessId source_process,
                     CefRefPtr<CefProcessMessage> message);

 private:
  bool Dispatch(const std::string& method,
                CefRefPtr<CefDictionaryValue> payload,
                CefRefPtr<CefValue>* result,
                std::string* error_code,
                std::string* error_message);

  void SendResponse(const std::string& id,
                    bool ok,
                    CefRefPtr<CefValue> result,
                    const std::string& error_code,
                    const std::string& error_message);
  void SendError(const std::string& id,
                 const std::string& code,
                 const std::string& message);

  BrowserWindow* window_;
  std::chrono::steady_clock::time_point rate_window_start_;
  int rate_window_count_ = 0;
};

}  // namespace nexus
