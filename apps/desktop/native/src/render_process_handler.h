#pragma once

#include <set>

#include "include/cef_render_process_handler.h"
#include "include/cef_v8.h"

namespace nexus {

// Renderer-process half of NEXUS IPC. The native bridge is installed only in
// main frames of the trusted shell browser (nexus://app).
class NexusRenderProcessHandler final : public CefRenderProcessHandler {
 public:
  NexusRenderProcessHandler() = default;

  void OnBrowserCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override;
  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override;

 private:
  std::set<int> shell_browser_ids_;

  IMPLEMENT_REFCOUNTING(NexusRenderProcessHandler);
  DISALLOW_COPY_AND_ASSIGN(NexusRenderProcessHandler);
};

}  // namespace nexus
