#include "src/asset_store.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <vector>

namespace nexus {
namespace {

constexpr size_t kMaxAssetBytes = 32 * 1024 * 1024;

std::string LowerExtension(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](char c) {
                   if (c >= 'A' && c <= 'Z') {
                     return static_cast<char>(c - 'A' + 'a');
                   }
                   return c;
                 });
  return extension;
}

std::string MimeTypeForExtension(std::string_view extension) {
  if (extension == ".html") {
    return "text/html; charset=utf-8";
  }
  if (extension == ".js" || extension == ".mjs") {
    return "text/javascript; charset=utf-8";
  }
  if (extension == ".css") {
    return "text/css; charset=utf-8";
  }
  if (extension == ".json" || extension == ".webmanifest") {
    return "application/json; charset=utf-8";
  }
  if (extension == ".svg") {
    return "image/svg+xml";
  }
  if (extension == ".png") {
    return "image/png";
  }
  if (extension == ".jpg" || extension == ".jpeg") {
    return "image/jpeg";
  }
  if (extension == ".webp") {
    return "image/webp";
  }
  if (extension == ".ico") {
    return "image/x-icon";
  }
  if (extension == ".woff2") {
    return "font/woff2";
  }
  if (extension == ".woff") {
    return "font/woff";
  }
  if (extension == ".ttf") {
    return "font/ttf";
  }
  if (extension == ".txt") {
    return "text/plain; charset=utf-8";
  }
  return {};
}

bool IsAllowedPathCharacter(char c) {
  const unsigned char byte = static_cast<unsigned char>(c);
  return std::isalnum(byte) != 0 || c == '-' || c == '_' || c == '.' ||
         c == '/' || c == '~';
}

// Returns a normalised relative path, or false when the URL path could escape
// the asset directory or uses an unsupported character/extension.
bool NormalizeUrlPath(std::string_view input, std::string* output) {
  if (input.size() > 512) {
    return false;
  }
  if (input.empty()) {
    *output = "index.html";
    return true;
  }
  if (input.front() == '/') {
    input.remove_prefix(1);
  }
  if (input.empty()) {
    *output = "index.html";
    return true;
  }
  if (input.find("//") != std::string_view::npos) {
    return false;
  }

  for (const char c : input) {
    if (!IsAllowedPathCharacter(c)) {
      return false;
    }
  }

  size_t start = 0;
  while (start <= input.size()) {
    const size_t slash = input.find('/', start);
    const size_t end =
        slash == std::string_view::npos ? input.size() : slash;
    const std::string_view segment = input.substr(start, end - start);
    if (segment.empty() || segment == "." || segment == "..") {
      return false;
    }
    if (slash == std::string_view::npos) {
      break;
    }
    start = slash + 1;
    if (start == input.size()) {
      // A trailing slash is a directory request, not a file.
      return false;
    }
  }

  *output = std::string(input);
  return true;
}

AssetResponse MakeError(int status,
                        std::string status_text,
                        std::string message) {
  AssetResponse response;
  response.status = status;
  response.status_text = std::move(status_text);
  response.mime_type = "text/plain; charset=utf-8";
  response.body = std::move(message);
  return response;
}

}  // namespace

AssetStore::AssetStore(std::filesystem::path ui_root)
    : ui_root_(std::move(ui_root)) {}

bool AssetStore::ready() const {
  std::error_code ec;
  return std::filesystem::is_regular_file(ui_root_ / L"index.html", ec);
}

AssetResponse AssetStore::Load(std::string_view host,
                               std::string_view url_path,
                               bool head_only) const {
  std::filesystem::path base;
  if (host == "app") {
    base = ui_root_;
  } else if (host == "newtab") {
    base = ui_root_ / L"newtab";
  } else {
    return MakeError(404, "Not Found", "Not found");
  }

  std::string relative;
  if (!NormalizeUrlPath(url_path, &relative)) {
    return MakeError(404, "Not Found", "Not found");
  }

  const std::filesystem::path relative_path =
      std::filesystem::path(std::string(relative));
  const std::string mime_type =
      MimeTypeForExtension(LowerExtension(relative_path));
  if (mime_type.empty()) {
    return MakeError(404, "Not Found", "Not found");
  }

  const std::filesystem::path file = base / relative_path;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file, ec)) {
    return MakeError(404, "Not Found", "Not found");
  }

  const uintmax_t size = std::filesystem::file_size(file, ec);
  if (ec || size > kMaxAssetBytes) {
    return MakeError(500, "Internal Server Error", "Asset unavailable");
  }

  std::ifstream stream(file, std::ios::binary);
  if (!stream) {
    return MakeError(500, "Internal Server Error", "Asset unavailable");
  }

  AssetResponse response;
  response.status = 200;
  response.status_text = "OK";
  response.mime_type = mime_type;
  response.body.assign(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
  response.head_only = head_only;
  if (response.body.size() != size) {
    return MakeError(500, "Internal Server Error", "Asset unavailable");
  }
  return response;
}

}  // namespace nexus
