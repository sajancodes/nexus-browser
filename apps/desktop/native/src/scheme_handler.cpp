#include "src/scheme_handler.h"

#include <cstring>
#include <fstream>
#include <string>
#include <utility>

#include "src/asset_store.h"
#include "include/cef_parser.h"
#include "include/cef_request.h"
#include "include/cef_resource_handler.h"
#include "include/cef_response.h"

namespace nexus {
namespace {

constexpr char kShellCsp[] =
    "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data:; font-src 'self'; connect-src 'none'; "
    "base-uri 'none'; form-action 'none'; frame-ancestors 'none'; "
    "object-src 'none'";

constexpr char kNewTabCsp[] =
    "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data:; font-src 'self'; connect-src 'none'; base-uri 'none'; "
    "form-action https://www.google.com https://google.com; frame-ancestors 'none'; "
    "object-src 'none'";

constexpr char kPermissionsPolicy[] =
    "camera=(), microphone=(), geolocation=(), payment=(), usb=(), "
    "serial=(), hid=(), midi=(), clipboard-read=(), clipboard-write=()";

std::string FormatContentType(const std::string& mime_type) {
  if (mime_type.empty()) {
    return {};
  }
  if (mime_type.rfind("text/", 0) == 0 ||
      mime_type == "application/json" ||
      mime_type == "image/svg+xml") {
    return mime_type + "; charset=utf-8";
  }
  return mime_type;
}

CefResponse::HeaderMap BuildHeaders(const std::string& host,
                                    const std::string& mime_type) {
  CefResponse::HeaderMap headers;
  const std::string content_type = FormatContentType(mime_type);
  if (!content_type.empty()) {
    headers.emplace("Content-Type", content_type);
  }
  headers.emplace("Content-Security-Policy",
                  host == "newtab" ? kNewTabCsp : kShellCsp);
  headers.emplace("X-Content-Type-Options", "nosniff");
  headers.emplace("Referrer-Policy", "no-referrer");
  headers.emplace("Cache-Control", "no-store");
  headers.emplace("Cross-Origin-Opener-Policy", "same-origin");
  headers.emplace("Cross-Origin-Resource-Policy", "same-origin");
  headers.emplace("X-Frame-Options", "DENY");
  headers.emplace("Permissions-Policy", kPermissionsPolicy);
  headers.emplace("X-Nexus-Resource", "local");
  return headers;
}

class MemoryResourceHandler final : public CefResourceHandler {
 public:
  explicit MemoryResourceHandler(AssetResponse asset,
                                 CefResponse::HeaderMap headers)
      : asset_(std::move(asset)), headers_(std::move(headers)) {}

  bool Open(CefRefPtr<CefRequest> /*request*/,
            bool& handle_request,
            CefRefPtr<CefCallback> /*callback*/) override {
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& /*redirectUrl*/) override {
    response->SetStatus(asset_.status);
    response->SetStatusText(CefString(asset_.status_text));
    response->SetMimeType(CefString(asset_.mime_type));
    if (asset_.mime_type.rfind("text/", 0) == 0 ||
        asset_.mime_type == "application/json" ||
        asset_.mime_type == "image/svg+xml") {
      response->SetCharset("utf-8");
    }

    CefResponse::HeaderMap headers = headers_;
    const std::string content_type = FormatContentType(asset_.mime_type);
    if (!content_type.empty() && headers.find("Content-Type") == headers.end()) {
      headers.emplace("Content-Type", content_type);
    }
    response->SetHeaderMap(headers);
    response_length = static_cast<int64_t>(asset_.body.size());
  }

  bool Skip(int64_t bytes_to_skip,
            int64_t& bytes_skipped,
            CefRefPtr<CefResourceSkipCallback> /*callback*/) override {
    if (bytes_to_skip < 0 ||
        offset_ + bytes_to_skip > static_cast<int64_t>(asset_.body.size())) {
      bytes_skipped = -2;
      return false;
    }
    offset_ += bytes_to_skip;
    bytes_skipped = bytes_to_skip;
    return true;
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> /*callback*/) override {
    const size_t remaining = asset_.body.size() - static_cast<size_t>(offset_);
    if (asset_.head_only || remaining == 0 || bytes_to_read <= 0) {
      bytes_read = 0;
      return false;
    }

    const size_t chunk = std::min(remaining,
                                  static_cast<size_t>(bytes_to_read));
    std::memcpy(data_out, asset_.body.data() + offset_, chunk);
    offset_ += static_cast<int64_t>(chunk);
    bytes_read = static_cast<int>(chunk);
    return true;
  }

  void Cancel() override {}

 private:
  AssetResponse asset_;
  CefResponse::HeaderMap headers_;
  int64_t offset_ = 0;

  IMPLEMENT_REFCOUNTING(MemoryResourceHandler);
  DISALLOW_COPY_AND_ASSIGN(MemoryResourceHandler);
};

CefRefPtr<CefResourceHandler> MakeErrorResponse(int status,
                                                const char* status_text,
                                                const char* message,
                                                const std::string& host) {
  AssetResponse asset;
  asset.status = status;
  asset.status_text = status_text;
  asset.mime_type = "text/plain";
  asset.body = message;
  const std::string mime = asset.mime_type;
  return new MemoryResourceHandler(std::move(asset), BuildHeaders(host, mime));
}

}  // namespace

NexusSchemeHandlerFactory::NexusSchemeHandlerFactory(
    std::shared_ptr<AssetStore> store)
    : store_(std::move(store)) {}

CefRefPtr<CefResourceHandler> NexusSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request) {
  const std::string scheme = scheme_name.ToString();
  if (scheme != "nexus" || !store_) {
    return nullptr;
  }

  const std::string method = request->GetMethod().ToString();
  if (method != "GET" && method != "HEAD") {
    return MakeErrorResponse(405, "Method Not Allowed",
                             "Only GET and HEAD are supported", "app");
  }

  CefURLParts parts;
  if (!CefParseURL(request->GetURL(), parts)) {
    return MakeErrorResponse(400, "Bad Request", "Invalid local URL", "app");
  }

  const std::string host = CefString(&parts.host).ToString();
  const std::string path = CefString(&parts.path).ToString();
  AssetResponse asset = store_->Load(host, path, method == "HEAD");
  const std::string mime = asset.mime_type;
  return new MemoryResourceHandler(std::move(asset), BuildHeaders(host, mime));
}

}  // namespace nexus
