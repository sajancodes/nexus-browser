#pragma once

#include <memory>

#include "include/cef_scheme.h"

namespace nexus {

class AssetStore;

// Serves the two local NEXUS origins. Registered for the `nexus` scheme on
// the global request context after CEF initialisation.
class NexusSchemeHandlerFactory final : public CefSchemeHandlerFactory {
 public:
  explicit NexusSchemeHandlerFactory(std::shared_ptr<AssetStore> store);

  CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) override;

 private:
  std::shared_ptr<AssetStore> store_;

  IMPLEMENT_REFCOUNTING(NexusSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(NexusSchemeHandlerFactory);
};

}  // namespace nexus
