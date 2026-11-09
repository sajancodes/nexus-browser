#include "src/privacy_service.h"

#include <algorithm>
#include <fstream>
#include <string>

#include "core/url_utils.h"
#include "include/cef_parser.h"

namespace nexus {
namespace {

std::string LowerAscii(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(), [](char c) {
    if (c >= 'A' && c <= 'Z') {
      return static_cast<char>(c - 'A' + 'a');
    }
    return c;
  });
  return result;
}

bool DomainMatches(std::string_view host, std::string_view domain) {
  if (host.empty() || domain.empty()) {
    return false;
  }
  const std::string normalized_host = LowerAscii(host);
  std::string normalized_domain = LowerAscii(domain);
  if (normalized_domain.front() == '.') {
    normalized_domain.erase(normalized_domain.begin());
  }
  if (normalized_domain.empty()) {
    return false;
  }
  if (normalized_host == normalized_domain) {
    return true;
  }
  return normalized_host.size() > normalized_domain.size() &&
         normalized_host.compare(normalized_host.size() -
                                     normalized_domain.size(),
                                 normalized_domain.size(),
                                 normalized_domain) == 0 &&
         normalized_host[normalized_host.size() - normalized_domain.size() -
                         1] == '.';
}

}  // namespace

CookieMode PrivacyService::CookieModeFromString(std::string_view value) {
  return value == "balanced" ? CookieMode::kBalanced : CookieMode::kStrict;
}

const char* PrivacyService::CookieModeToString(CookieMode mode) {
  return mode == CookieMode::kBalanced ? "balanced" : "strict";
}

void PrivacyService::LoadPreferences(
    const std::filesystem::path& preferences_path) {
  preferences_path_ = preferences_path;
  mode_.store(static_cast<int>(CookieMode::kStrict),
              std::memory_order_relaxed);

  std::ifstream file(preferences_path_, std::ios::binary);
  if (!file) {
    return;
  }

  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  if (contents.size() > 64 * 1024) {
    return;
  }

  CefRefPtr<CefValue> root = CefParseJSON(
      CefString(contents), JSON_PARSER_RFC);
  if (!root || root->GetType() != VTYPE_DICTIONARY) {
    return;
  }

  CefRefPtr<CefDictionaryValue> dictionary = root->GetDictionary();
  if (!dictionary || !dictionary->HasKey("cookieMode")) {
    return;
  }
  const CefString value = dictionary->GetString("cookieMode");
  if (value.empty()) {
    return;
  }
  mode_.store(
      static_cast<int>(CookieModeFromString(value.ToString())),
      std::memory_order_relaxed);
}

bool PrivacyService::SetCookieMode(CookieMode mode) {
  const int next = static_cast<int>(mode);
  const int previous = mode_.exchange(next, std::memory_order_relaxed);
  SavePreferences();
  return previous != next;
}

void PrivacyService::SavePreferences() {
  if (preferences_path_.empty()) {
    return;
  }

  std::ofstream file(preferences_path_, std::ios::binary | std::ios::trunc);
  if (!file) {
    return;
  }
  file << "{\n  \"v\": 1,\n  \"cookieMode\": \""
       << CookieModeToString(mode()) << "\"\n}\n";
}

bool PrivacyService::ShouldAllowCookie(CefRefPtr<CefRequest> request,
                                       const CefCookie& cookie) {
  if (mode() == CookieMode::kStrict) {
    RecordBlockedCookie();
    return false;
  }

  // Balanced mode permits only cookies whose domain matches the top-level
  // site for the request. Cross-site (third-party) cookie reads and writes are
  // denied. Host-only cookies with an empty domain are judged against the
  // request URL.
  std::string first_party = request->GetFirstPartyForCookies().ToString();
  std::string first_party_host = core::HostOfUrl(first_party);
  const std::string request_url = request->GetURL().ToString();

  if (first_party_host.empty()) {
    first_party_host = core::HostOfUrl(request_url);
  }

  const std::string cookie_domain = CefString(&cookie.domain).ToString();
  if (first_party_host.empty() || cookie_domain.empty()) {
    RecordBlockedCookie();
    return false;
  }

  if (!DomainMatches(first_party_host, cookie_domain)) {
    RecordBlockedCookie();
    return false;
  }
  return true;
}

}  // namespace nexus
