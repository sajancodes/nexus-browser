#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace nexus::privacy {

class FilterEngine {
 public:
  FilterEngine();
  ~FilterEngine();

  // Returns true if the request should be blocked.
  bool ShouldBlock(std::string_view url, std::string_view initiator_host);

  // Load rules
  void AddBlockDomain(std::string_view domain);
  void AddCustomRule(const std::string& pattern);
  void LoadDefaultRules();
  void LoadDatabaseRules(const std::vector<std::string>& rules);

  size_t RuleCount() const;

 private:
  bool MatchesDomain(std::string_view host) const;

  std::unordered_set<std::string> blocked_domains_;
  std::vector<std::string> blocked_substrings_;
  mutable std::mutex mutex_;
};

}  // namespace nexus::privacy
