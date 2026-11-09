#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nexus {

struct AssetResponse {
  int status = 404;
  std::string status_text = "Not Found";
  std::string mime_type = "text/plain";
  std::string body;
  bool head_only = false;
};

// Serves the packaged NEXUS shell from <exe>/ui. Only `nexus://app` and
// `nexus://newtab` are mapped, and only regular files with known web
// extensions can be read.
class AssetStore {
 public:
  explicit AssetStore(std::filesystem::path ui_root);

  bool ready() const;
  AssetResponse Load(std::string_view host,
                     std::string_view url_path,
                     bool head_only) const;

 private:
  std::filesystem::path ui_root_;
};

}  // namespace nexus
