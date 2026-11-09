#include "src/privacy/filter_engine.h"

#include <algorithm>
#include <cctype>

#include "core/url_utils.h"

namespace nexus::privacy {
namespace {

std::string ToLower(std::string_view s) {
  std::string result(s);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

const char* const kDefaultTrackerDomains[] = {
    // Major ad networks
    "doubleclick.net",
    "googlesyndication.com",
    "googleadservices.com",
    "adservice.google.com",
    "pagead2.googlesyndication.com",
    "adnxs.com",
    "advertising.com",
    "rubiconproject.com",
    "pubmatic.com",
    "openx.net",
    "casalemedia.com",
    "adroll.com",
    "smartadserver.com",
    "moatads.com",
    "amazon-adsystem.com",
    "criteo.com",
    "criteo.net",
    "outbrain.com",
    "taboola.com",
    "bidswitch.net",
    "adsrvr.org",
    "serving-sys.com",
    "spotxchange.com",
    "yieldmo.com",

    // Tracking, Telemetry & Analytics
    "google-analytics.com",
    "googletagmanager.com",
    "pixel.facebook.com",
    "connect.facebook.net",
    "scorecardresearch.com",
    "quantserve.com",
    "hotjar.com",
    "fullstory.com",
    "mouseflow.com",
    "mixpanel.com",
    "segment.io",
    "segment.com",
    "amplitude.com",
    "appsflyer.com",
    "branch.io",
    "chartbeat.com",
    "optimizely.com",
    "newrelic.com",
    "nr-data.net",
    "sentry.io",
    "bugsnag.com",
    "statcounter.com",
    "yandex.ru/metrika",
    "mc.yandex.ru",
};

}  // namespace

FilterEngine::FilterEngine() {
  LoadDefaultRules();
}

FilterEngine::~FilterEngine() = default;

void FilterEngine::LoadDefaultRules() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const char* domain : kDefaultTrackerDomains) {
    blocked_domains_.insert(ToLower(domain));
  }

  // Common tracking substrings
  blocked_substrings_ = {
      "/google-analytics.js",
      "/gtag/js",
      "/gtm.js",
      "/fbevents.js",
      "/pixel.js",
      "/advertisement.",
      "/ads.js",
      "/track.js",
      "/telemetry.js",
      "/telemetry/",
      "/adserver/",
      "/collect/beacon",
      "beacon.min.js",
  };
}

void FilterEngine::AddBlockDomain(std::string_view domain) {
  std::lock_guard<std::mutex> lock(mutex_);
  blocked_domains_.insert(ToLower(domain));
}

void FilterEngine::AddCustomRule(const std::string& pattern) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (pattern.empty()) return;
  if (pattern.front() == '|' || pattern.find('/') != std::string::npos) {
    blocked_substrings_.push_back(ToLower(pattern));
  } else {
    blocked_domains_.insert(ToLower(pattern));
  }
}

void FilterEngine::LoadDatabaseRules(const std::vector<std::string>& rules) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& r : rules) {
    if (r.empty()) continue;
    if (r.find('/') != std::string::npos) {
      blocked_substrings_.push_back(ToLower(r));
    } else {
      blocked_domains_.insert(ToLower(r));
    }
  }
}

size_t FilterEngine::RuleCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return blocked_domains_.size() + blocked_substrings_.size();
}

bool FilterEngine::MatchesDomain(std::string_view host) const {
  if (host.empty()) return false;
  std::string lower_host = ToLower(host);

  // Exact match
  if (blocked_domains_.find(lower_host) != blocked_domains_.end()) {
    return true;
  }

  // Check parent domains (e.g. ad.doubleclick.net matches doubleclick.net)
  size_t dot_pos = lower_host.find('.');
  while (dot_pos != std::string::npos && dot_pos < lower_host.size() - 1) {
    std::string sub = lower_host.substr(dot_pos + 1);
    if (blocked_domains_.find(sub) != blocked_domains_.end()) {
      return true;
    }
    dot_pos = lower_host.find('.', dot_pos + 1);
  }

  return false;
}

bool FilterEngine::ShouldBlock(std::string_view url, std::string_view /*initiator_host*/) {
  if (url.empty()) return false;

  const std::string host = core::HostOfUrl(url);
  if (host.empty()) return false;

  std::lock_guard<std::mutex> lock(mutex_);

  // Check domain match
  if (MatchesDomain(host)) {
    return true;
  }

  // Check URL substrings
  const std::string lower_url = ToLower(url);
  for (const auto& sub : blocked_substrings_) {
    if (lower_url.find(sub) != std::string::npos) {
      return true;
    }
  }

  return false;
}

}  // namespace nexus::privacy
