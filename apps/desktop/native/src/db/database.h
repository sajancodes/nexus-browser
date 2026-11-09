#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;

namespace nexus::db {

struct HistoryItem {
  int64_t id = 0;
  std::string url;
  std::string title;
  int64_t visit_time = 0;
  int visit_count = 1;
};

struct BookmarkItem {
  int64_t id = 0;
  std::string url;
  std::string title;
  int64_t created_at = 0;
};

struct DownloadRecord {
  int64_t id = 0;
  std::string guid;
  std::string url;
  std::string file_name;
  std::string file_path;
  int64_t total_bytes = 0;
  int64_t received_bytes = 0;
  std::string state; // "in_progress", "completed", "cancelled", "failed", "paused"
  int64_t created_at = 0;
  int64_t completed_at = 0;
};

struct MediaItem {
  int64_t id = 0;
  std::string url;
  std::string source_url;
  std::string media_type; // "video", "audio", "image", "stream"
  std::string title;
  int64_t size_bytes = 0;
  int64_t discovered_at = 0;
};

struct VpnConfig {
  int64_t id = 0;
  std::string name;
  std::string config_content;
  std::string interface_address;
  std::string endpoint;
  int64_t created_at = 0;
  bool is_active = false;
};

struct SitePermission {
  std::string origin;
  std::string permission;
  std::string state; // "allow", "deny", "prompt"
  int64_t updated_at = 0;
};

class Database {
 public:
  Database();
  ~Database();

  bool Open(const std::filesystem::path& db_path);
  void Close();
  bool IsOpen() const;

  // History operations
  bool AddHistory(const std::string& url, const std::string& title);
  std::vector<HistoryItem> GetHistory(int limit = 100, const std::string& query = "");
  std::vector<HistoryItem> SearchAutocomplete(const std::string& prefix, int limit = 8);
  bool DeleteHistoryItem(int64_t id);
  bool ClearHistory(int64_t older_than_ms = 0);

  // Bookmark operations
  bool AddBookmark(const std::string& url, const std::string& title);
  bool RemoveBookmark(const std::string& url);
  bool IsBookmarked(const std::string& url);
  std::vector<BookmarkItem> GetBookmarks();

  // Download operations
  bool UpsertDownload(const DownloadRecord& record);
  std::vector<DownloadRecord> GetDownloads(int limit = 100);
  bool DeleteDownload(const std::string& guid);

  // Media items
  bool AddMediaItem(const MediaItem& item);
  std::vector<MediaItem> GetMediaItems(const std::string& source_url = "", int limit = 50);
  bool ClearMediaItems();

  // VPN configurations
  bool AddVpnConfig(const std::string& name, const std::string& content,
                    const std::string& iface_addr, const std::string& endpoint);
  std::vector<VpnConfig> GetVpnConfigs();
  bool DeleteVpnConfig(int64_t id);
  bool SetActiveVpnConfig(int64_t id);

  // Site permissions
  bool SetSitePermission(const std::string& origin, const std::string& permission, const std::string& state);
  std::string GetSitePermission(const std::string& origin, const std::string& permission);
  std::vector<SitePermission> GetSitePermissions();
  bool ClearSitePermissions(const std::string& origin = "");

  // Settings key-value
  std::string GetSetting(const std::string& key, const std::string& default_value = "");
  bool SetSetting(const std::string& key, const std::string& value);

  // Filter rules
  std::vector<std::string> GetFilterRules();
  bool AddFilterRule(const std::string& pattern, bool is_block = true);

 private:
  bool InitializeSchema();
  bool Execute(const char* sql);

  sqlite3* db_ = nullptr;
  mutable std::mutex mutex_;
};

}  // namespace nexus::db
