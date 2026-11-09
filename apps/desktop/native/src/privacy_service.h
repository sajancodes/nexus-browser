#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "include/cef_base.h"
#include "include/cef_cookie.h"
#include "include/cef_request.h"

namespace nexus {

enum class CookieMode {
  kStrict = 0,
  kBalanced = 1,
};

// Thread-safe privacy policy and counters shared by the browser process UI
// thread and CEF's IO thread.
class PrivacyService final : public CefBaseRefCounted {
 public:
  PrivacyService() = default;

  static CookieMode CookieModeFromString(std::string_view value);
  static const char* CookieModeToString(CookieMode mode);

  // Reads the persisted preference at |preferences_path|. Missing or invalid
  // files fall back to strict mode.
  void LoadPreferences(const std::filesystem::path& preferences_path);

  // Updates the mode and persists it. Returns true when the mode changed.
  bool SetCookieMode(CookieMode mode);

  CookieMode mode() const {
    return static_cast<CookieMode>(mode_.load(std::memory_order_relaxed));
  }

  // Called from CefCookieAccessFilter on the IO thread.
  bool ShouldAllowCookie(CefRefPtr<CefRequest> request,
                         const CefCookie& cookie);

  void RecordBlockedCookie() {
    blocked_cookies_.fetch_add(1, std::memory_order_relaxed);
  }
  void RecordDeniedPermission() {
    denied_permissions_.fetch_add(1, std::memory_order_relaxed);
  }
  void RecordBlockedDownload() {
    blocked_downloads_.fetch_add(1, std::memory_order_relaxed);
  }
  void RecordStrippedReferrer() {
    stripped_referrers_.fetch_add(1, std::memory_order_relaxed);
  }
  void RecordBlockedPopup() {
    blocked_popups_.fetch_add(1, std::memory_order_relaxed);
  }
  void RecordBlockedNavigation() {
    blocked_navigations_.fetch_add(1, std::memory_order_relaxed);
  }
  void RecordDeletedCookies(int count) {
    if (count > 0) {
      deleted_cookies_.fetch_add(static_cast<uint64_t>(count),
                                 std::memory_order_relaxed);
    }
  }

  uint64_t blocked_cookies() const {
    return blocked_cookies_.load(std::memory_order_relaxed);
  }
  uint64_t denied_permissions() const {
    return denied_permissions_.load(std::memory_order_relaxed);
  }
  uint64_t blocked_downloads() const {
    return blocked_downloads_.load(std::memory_order_relaxed);
  }
  uint64_t stripped_referrers() const {
    return stripped_referrers_.load(std::memory_order_relaxed);
  }
  uint64_t blocked_popups() const {
    return blocked_popups_.load(std::memory_order_relaxed);
  }
  uint64_t blocked_navigations() const {
    return blocked_navigations_.load(std::memory_order_relaxed);
  }
  uint64_t deleted_cookies() const {
    return deleted_cookies_.load(std::memory_order_relaxed);
  }

 private:
  void SavePreferences();

  std::filesystem::path preferences_path_;
  std::atomic<int> mode_{static_cast<int>(CookieMode::kStrict)};
  std::atomic<uint64_t> blocked_cookies_{0};
  std::atomic<uint64_t> denied_permissions_{0};
  std::atomic<uint64_t> blocked_downloads_{0};
  std::atomic<uint64_t> stripped_referrers_{0};
  std::atomic<uint64_t> blocked_popups_{0};
  std::atomic<uint64_t> blocked_navigations_{0};
  std::atomic<uint64_t> deleted_cookies_{0};

  IMPLEMENT_REFCOUNTING(PrivacyService);
  DISALLOW_COPY_AND_ASSIGN(PrivacyService);
};

}  // namespace nexus
