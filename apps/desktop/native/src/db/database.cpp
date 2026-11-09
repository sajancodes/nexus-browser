#include "src/db/database.h"

#include <chrono>
#include <iostream>

#include "core/data_paths.h"
#include "third_party/sqlite/sqlite3.h"

namespace nexus::db {
namespace {

int64_t CurrentTimeMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

inline std::string ColumnText(sqlite3_stmt* stmt, int col) {
  const unsigned char* text = sqlite3_column_text(stmt, col);
  return text ? reinterpret_cast<const char*>(text) : "";
}

}  // namespace

Database::Database() = default;

Database::~Database() {
  Close();
}

bool Database::Open(const std::filesystem::path& db_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_) {
    return true;
  }

  const std::string path_str = core::PathToUtf8(db_path);
  int rc = sqlite3_open_v2(path_str.c_str(), &db_,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                           nullptr);
  if (rc != SQLITE_OK || !db_) {
    std::cerr << "Failed to open SQLite database: "
              << (db_ ? sqlite3_errmsg(db_) : "allocation error") << "\n";
    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    return false;
  }

  // Configure SQLite for high concurrency, durability, and reliability.
  sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);

  return InitializeSchema();
}

void Database::Close() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

bool Database::IsOpen() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return db_ != nullptr;
}

bool Database::Execute(const char* sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    if (err_msg) {
      std::cerr << "SQLite error: " << err_msg << "\n";
      sqlite3_free(err_msg);
    }
    return false;
  }
  return true;
}

bool Database::InitializeSchema() {
  const char* schema = R"SQL(
    CREATE TABLE IF NOT EXISTS history (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      url TEXT NOT NULL,
      title TEXT NOT NULL,
      visit_time INTEGER NOT NULL,
      visit_count INTEGER NOT NULL DEFAULT 1
    );
    CREATE INDEX IF NOT EXISTS idx_history_url ON history(url);
    CREATE INDEX IF NOT EXISTS idx_history_time ON history(visit_time DESC);

    CREATE TABLE IF NOT EXISTS bookmarks (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      url TEXT NOT NULL UNIQUE,
      title TEXT NOT NULL,
      created_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_bookmarks_url ON bookmarks(url);

    CREATE TABLE IF NOT EXISTS downloads (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      guid TEXT NOT NULL UNIQUE,
      url TEXT NOT NULL,
      file_name TEXT NOT NULL,
      file_path TEXT NOT NULL,
      total_bytes INTEGER NOT NULL,
      received_bytes INTEGER NOT NULL,
      state TEXT NOT NULL,
      created_at INTEGER NOT NULL,
      completed_at INTEGER DEFAULT 0
    );
    CREATE INDEX IF NOT EXISTS idx_downloads_time ON downloads(created_at DESC);

    CREATE TABLE IF NOT EXISTS media_items (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      url TEXT NOT NULL,
      source_url TEXT NOT NULL,
      media_type TEXT NOT NULL,
      title TEXT,
      size_bytes INTEGER DEFAULT 0,
      discovered_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_media_source ON media_items(source_url);

    CREATE TABLE IF NOT EXISTS vpn_configs (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL UNIQUE,
      config_content TEXT NOT NULL,
      interface_address TEXT,
      endpoint TEXT,
      created_at INTEGER NOT NULL,
      is_active INTEGER NOT NULL DEFAULT 0
    );

    CREATE TABLE IF NOT EXISTS site_permissions (
      origin TEXT NOT NULL,
      permission TEXT NOT NULL,
      state TEXT NOT NULL,
      updated_at INTEGER NOT NULL,
      PRIMARY KEY (origin, permission)
    );

    CREATE TABLE IF NOT EXISTS filter_rules (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      pattern TEXT NOT NULL UNIQUE,
      is_block INTEGER NOT NULL DEFAULT 1,
      created_at INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS settings (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );
  )SQL";

  return Execute(schema);
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

bool Database::AddHistory(const std::string& url, const std::string& title) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || url.empty()) return false;

  const int64_t now = CurrentTimeMs();

  // Check if URL was visited recently: if so, increment visit_count and update visit_time
  const char* check_sql = "SELECT id, visit_count FROM history WHERE url = ? ORDER BY visit_time DESC LIMIT 1;";
  sqlite3_stmt* check_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, check_sql, -1, &check_stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(check_stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(check_stmt) == SQLITE_ROW) {
      int64_t id = sqlite3_column_int64(check_stmt, 0);
      int count = sqlite3_column_int(check_stmt, 1);
      sqlite3_finalize(check_stmt);

      const char* update_sql = "UPDATE history SET visit_time = ?, visit_count = ?, title = ? WHERE id = ?;";
      sqlite3_stmt* upd_stmt = nullptr;
      if (sqlite3_prepare_v2(db_, update_sql, -1, &upd_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(upd_stmt, 1, now);
        sqlite3_bind_int(upd_stmt, 2, count + 1);
        sqlite3_bind_text(upd_stmt, 3, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(upd_stmt, 4, id);
        sqlite3_step(upd_stmt);
        sqlite3_finalize(upd_stmt);
        return true;
      }
      return false;
    }
    sqlite3_finalize(check_stmt);
  }

  const char* sql = "INSERT INTO history (url, title, visit_time, visit_count) VALUES (?, ?, ?, 1);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, now);

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<HistoryItem> Database::GetHistory(int limit, const std::string& query) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<HistoryItem> results;
  if (!db_) return results;

  std::string sql;
  bool has_query = !query.empty();
  if (has_query) {
    sql = "SELECT id, url, title, visit_time, visit_count FROM history "
          "WHERE url LIKE ? OR title LIKE ? ORDER BY visit_time DESC LIMIT ?;";
  } else {
    sql = "SELECT id, url, title, visit_time, visit_count FROM history "
          "ORDER BY visit_time DESC LIMIT ?;";
  }

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return results;
  }

  if (has_query) {
    std::string wildcard = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, wildcard.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, wildcard.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, limit);
  } else {
    sqlite3_bind_int(stmt, 1, limit);
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    HistoryItem item;
    item.id = sqlite3_column_int64(stmt, 0);
    item.url = ColumnText(stmt, 1);
    item.title = ColumnText(stmt, 2);
    item.visit_time = sqlite3_column_int64(stmt, 3);
    item.visit_count = sqlite3_column_int(stmt, 4);
    results.push_back(std::move(item));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::vector<HistoryItem> Database::SearchAutocomplete(const std::string& prefix, int limit) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<HistoryItem> results;
  if (!db_ || prefix.empty()) return results;

  const char* sql =
      "SELECT id, url, title, visit_time, visit_count FROM history "
      "WHERE url LIKE ? OR title LIKE ? ORDER BY visit_count DESC, visit_time DESC LIMIT ?;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

  std::string wildcard = "%" + prefix + "%";
  sqlite3_bind_text(stmt, 1, wildcard.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, wildcard.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    HistoryItem item;
    item.id = sqlite3_column_int64(stmt, 0);
    item.url = ColumnText(stmt, 1);
    item.title = ColumnText(stmt, 2);
    item.visit_time = sqlite3_column_int64(stmt, 3);
    item.visit_count = sqlite3_column_int(stmt, 4);
    results.push_back(std::move(item));
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::DeleteHistoryItem(int64_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  const char* sql = "DELETE FROM history WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int64(stmt, 1, id);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool Database::ClearHistory(int64_t older_than_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  if (older_than_ms == 0) {
    return Execute("DELETE FROM history;");
  }

  const char* sql = "DELETE FROM history WHERE visit_time <= ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int64(stmt, 1, older_than_ms);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

// ---------------------------------------------------------------------------
// Bookmarks
// ---------------------------------------------------------------------------

bool Database::AddBookmark(const std::string& url, const std::string& title) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || url.empty()) return false;

  const char* sql = "INSERT INTO bookmarks (url, title, created_at) VALUES (?, ?, ?) "
                    "ON CONFLICT(url) DO UPDATE SET title = excluded.title;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, CurrentTimeMs());

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool Database::RemoveBookmark(const std::string& url) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  const char* sql = "DELETE FROM bookmarks WHERE url = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool Database::IsBookmarked(const std::string& url) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || url.empty()) return false;

  const char* sql = "SELECT 1 FROM bookmarks WHERE url = ? LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

  bool bookmarked = (sqlite3_step(stmt) == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return bookmarked;
}

std::vector<BookmarkItem> Database::GetBookmarks() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<BookmarkItem> results;
  if (!db_) return results;

  const char* sql = "SELECT id, url, title, created_at FROM bookmarks ORDER BY title ASC, created_at DESC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    BookmarkItem item;
    item.id = sqlite3_column_int64(stmt, 0);
    item.url = ColumnText(stmt, 1);
    item.title = ColumnText(stmt, 2);
    item.created_at = sqlite3_column_int64(stmt, 3);
    results.push_back(std::move(item));
  }

  sqlite3_finalize(stmt);
  return results;
}

// ---------------------------------------------------------------------------
// Downloads
// ---------------------------------------------------------------------------

bool Database::UpsertDownload(const DownloadRecord& record) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || record.guid.empty()) return false;

  const char* sql =
      "INSERT INTO downloads (guid, url, file_name, file_path, total_bytes, received_bytes, state, created_at, completed_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(guid) DO UPDATE SET "
      "received_bytes = excluded.received_bytes, total_bytes = excluded.total_bytes, state = excluded.state, completed_at = excluded.completed_at;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, record.guid.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, record.url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, record.file_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, record.file_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, record.total_bytes);
  sqlite3_bind_int64(stmt, 6, record.received_bytes);
  sqlite3_bind_text(stmt, 7, record.state.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 8, record.created_at > 0 ? record.created_at : CurrentTimeMs());
  sqlite3_bind_int64(stmt, 9, record.completed_at);

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<DownloadRecord> Database::GetDownloads(int limit) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<DownloadRecord> results;
  if (!db_) return results;

  const char* sql =
      "SELECT id, guid, url, file_name, file_path, total_bytes, received_bytes, state, created_at, completed_at "
      "FROM downloads ORDER BY created_at DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

  sqlite3_bind_int(stmt, 1, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    DownloadRecord rec;
    rec.id = sqlite3_column_int64(stmt, 0);
    rec.guid = ColumnText(stmt, 1);
    rec.url = ColumnText(stmt, 2);
    rec.file_name = ColumnText(stmt, 3);
    rec.file_path = ColumnText(stmt, 4);
    rec.total_bytes = sqlite3_column_int64(stmt, 5);
    rec.received_bytes = sqlite3_column_int64(stmt, 6);
    rec.state = ColumnText(stmt, 7);
    rec.created_at = sqlite3_column_int64(stmt, 8);
    rec.completed_at = sqlite3_column_int64(stmt, 9);
    results.push_back(std::move(rec));
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::DeleteDownload(const std::string& guid) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  const char* sql = "DELETE FROM downloads WHERE guid = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, guid.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

// ---------------------------------------------------------------------------
// Media Items
// ---------------------------------------------------------------------------

bool Database::AddMediaItem(const MediaItem& item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || item.url.empty()) return false;

  const char* sql =
      "INSERT INTO media_items (url, source_url, media_type, title, size_bytes, discovered_at) "
      "VALUES (?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, item.url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, item.source_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, item.media_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, item.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, item.size_bytes);
  sqlite3_bind_int64(stmt, 6, item.discovered_at > 0 ? item.discovered_at : CurrentTimeMs());

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<MediaItem> Database::GetMediaItems(const std::string& source_url, int limit) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<MediaItem> results;
  if (!db_) return results;

  std::string sql;
  bool filter_source = !source_url.empty();
  if (filter_source) {
    sql = "SELECT id, url, source_url, media_type, title, size_bytes, discovered_at "
          "FROM media_items WHERE source_url = ? ORDER BY discovered_at DESC LIMIT ?;";
  } else {
    sql = "SELECT id, url, source_url, media_type, title, size_bytes, discovered_at "
          "FROM media_items ORDER BY discovered_at DESC LIMIT ?;";
  }

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

  if (filter_source) {
    sqlite3_bind_text(stmt, 1, source_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
  } else {
    sqlite3_bind_int(stmt, 1, limit);
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    MediaItem mi;
    mi.id = sqlite3_column_int64(stmt, 0);
    mi.url = ColumnText(stmt, 1);
    mi.source_url = ColumnText(stmt, 2);
    mi.media_type = ColumnText(stmt, 3);
    mi.title = ColumnText(stmt, 4);
    mi.size_bytes = sqlite3_column_int64(stmt, 5);
    mi.discovered_at = sqlite3_column_int64(stmt, 6);
    results.push_back(std::move(mi));
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::ClearMediaItems() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;
  return Execute("DELETE FROM media_items;");
}

// ---------------------------------------------------------------------------
// VPN Configurations
// ---------------------------------------------------------------------------

bool Database::AddVpnConfig(const std::string& name, const std::string& content,
                            const std::string& iface_addr, const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || name.empty()) return false;

  const char* sql =
      "INSERT INTO vpn_configs (name, config_content, interface_address, endpoint, created_at, is_active) "
      "VALUES (?, ?, ?, ?, ?, 0) "
      "ON CONFLICT(name) DO UPDATE SET config_content = excluded.config_content, "
      "interface_address = excluded.interface_address, endpoint = excluded.endpoint;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, iface_addr.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, endpoint.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, CurrentTimeMs());

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<VpnConfig> Database::GetVpnConfigs() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<VpnConfig> results;
  if (!db_) return results;

  const char* sql = "SELECT id, name, config_content, interface_address, endpoint, created_at, is_active "
                    "FROM vpn_configs ORDER BY created_at DESC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    VpnConfig c;
    c.id = sqlite3_column_int64(stmt, 0);
    c.name = ColumnText(stmt, 1);
    c.config_content = ColumnText(stmt, 2);
    c.interface_address = ColumnText(stmt, 3);
    c.endpoint = ColumnText(stmt, 4);
    c.created_at = sqlite3_column_int64(stmt, 5);
    c.is_active = (sqlite3_column_int(stmt, 6) != 0);
    results.push_back(std::move(c));
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::DeleteVpnConfig(int64_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  const char* sql = "DELETE FROM vpn_configs WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int64(stmt, 1, id);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool Database::SetActiveVpnConfig(int64_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  sqlite3_exec(db_, "UPDATE vpn_configs SET is_active = 0;", nullptr, nullptr, nullptr);
  if (id <= 0) return true;

  const char* sql = "UPDATE vpn_configs SET is_active = 1 WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_int64(stmt, 1, id);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

// ---------------------------------------------------------------------------
// Site Permissions
// ---------------------------------------------------------------------------

bool Database::SetSitePermission(const std::string& origin, const std::string& permission, const std::string& state) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || origin.empty() || permission.empty()) return false;

  const char* sql =
      "INSERT INTO site_permissions (origin, permission, state, updated_at) "
      "VALUES (?, ?, ?, ?) "
      "ON CONFLICT(origin, permission) DO UPDATE SET state = excluded.state, updated_at = excluded.updated_at;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, origin.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, permission.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, state.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, CurrentTimeMs());

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

std::string Database::GetSitePermission(const std::string& origin, const std::string& permission) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || origin.empty() || permission.empty()) return "deny";

  const char* sql = "SELECT state FROM site_permissions WHERE origin = ? AND permission = ? LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "deny";

  sqlite3_bind_text(stmt, 1, origin.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, permission.c_str(), -1, SQLITE_TRANSIENT);

  std::string result = "deny";
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* s = sqlite3_column_text(stmt, 0);
    if (s) result = reinterpret_cast<const char*>(s);
  }

  sqlite3_finalize(stmt);
  return result;
}

std::vector<SitePermission> Database::GetSitePermissions() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<SitePermission> results;
  if (!db_) return results;

  const char* sql = "SELECT origin, permission, state, updated_at FROM site_permissions ORDER BY origin ASC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SitePermission sp;
    sp.origin = ColumnText(stmt, 0);
    sp.permission = ColumnText(stmt, 1);
    sp.state = ColumnText(stmt, 2);
    sp.updated_at = sqlite3_column_int64(stmt, 3);
    results.push_back(std::move(sp));
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::ClearSitePermissions(const std::string& origin) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_) return false;

  if (origin.empty()) {
    return Execute("DELETE FROM site_permissions;");
  }

  const char* sql = "DELETE FROM site_permissions WHERE origin = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, origin.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

// ---------------------------------------------------------------------------
// Settings Key-Value
// ---------------------------------------------------------------------------

std::string Database::GetSetting(const std::string& key, const std::string& default_value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || key.empty()) return default_value;

  const char* sql = "SELECT value FROM settings WHERE key = ? LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return default_value;

  sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
  std::string val = default_value;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* s = sqlite3_column_text(stmt, 0);
    if (s) val = reinterpret_cast<const char*>(s);
  }

  sqlite3_finalize(stmt);
  return val;
}

bool Database::SetSetting(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || key.empty()) return false;

  const char* sql = "INSERT INTO settings (key, value) VALUES (?, ?) "
                    "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

// ---------------------------------------------------------------------------
// Filter Rules
// ---------------------------------------------------------------------------

std::vector<std::string> Database::GetFilterRules() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> results;
  if (!db_) return results;

  const char* sql = "SELECT pattern FROM filter_rules WHERE is_block = 1 ORDER BY id ASC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* p = sqlite3_column_text(stmt, 0);
    if (p) results.emplace_back(reinterpret_cast<const char*>(p));
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::AddFilterRule(const std::string& pattern, bool is_block) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_ || pattern.empty()) return false;

  const char* sql = "INSERT OR IGNORE INTO filter_rules (pattern, is_block, created_at) VALUES (?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, is_block ? 1 : 0);
  sqlite3_bind_int64(stmt, 3, CurrentTimeMs());

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

}  // namespace nexus::db
