#include "core/url_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace nexus::core {
namespace {

constexpr std::string_view kSearchPrefix =
    "https://duckduckgo.com/?q=";
constexpr std::string_view kNewTabUrl = "nexus://newtab/";

char LowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') {
    return static_cast<char>(value - 'A' + 'a');
  }
  return value;
}

std::string Lower(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (char c : value) {
    result.push_back(LowerAscii(c));
  }
  return result;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

bool StartsWithCI(std::string_view value, std::string_view prefix) {
  if (value.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (LowerAscii(value[i]) != LowerAscii(prefix[i])) {
      return false;
    }
  }
  return true;
}

bool IsSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string Trim(std::string_view value) {
  size_t start = 0;
  size_t end = value.size();
  while (start < end && IsSpace(value[start])) {
    ++start;
  }
  while (end > start && IsSpace(value[end - 1])) {
    --end;
  }
  return std::string(value.substr(start, end - start));
}

bool HasControl(std::string_view value) {
  for (char c : value) {
    const unsigned char byte = static_cast<unsigned char>(c);
    if (byte < 0x20 || byte == 0x7F) {
      return true;
    }
  }
  return false;
}

bool IsSchemeCharacter(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '+' ||
         c == '-' || c == '.';
}

// Returns the lowercase scheme when |value| begins with a syntactically valid
// scheme followed by a colon. Note that "localhost:8080" also matches this
// shape; callers disambiguate host:port separately.
std::string SchemeOf(std::string_view value) {
  if (value.empty() || std::isalpha(static_cast<unsigned char>(value[0])) == 0) {
    return {};
  }
  for (size_t i = 1; i < value.size(); ++i) {
    const char c = value[i];
    if (c == ':') {
      return Lower(value.substr(0, i));
    }
    if (!IsSchemeCharacter(c)) {
      return {};
    }
  }
  return {};
}

std::string_view AuthorityOf(std::string_view url) {
  const size_t scheme_end = url.find("://");
  if (scheme_end == std::string_view::npos) {
    return {};
  }
  const size_t authority_start = scheme_end + 3;
  const size_t authority_end = url.find_first_of("/?#", authority_start);
  if (authority_end == std::string_view::npos) {
    return url.substr(authority_start);
  }
  return url.substr(authority_start, authority_end - authority_start);
}

bool IsDigits(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  for (char c : value) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

struct HostPort {
  std::string_view host;
  std::string_view port;
  bool has_userinfo = false;
};

HostPort SplitHostPort(std::string_view authority) {
  HostPort result;
  const size_t at = authority.rfind('@');
  if (at != std::string_view::npos) {
    result.has_userinfo = true;
    authority = authority.substr(at + 1);
  }

  if (!authority.empty() && authority.front() == '[') {
    const size_t close = authority.find(']');
    if (close != std::string_view::npos) {
      result.host = authority.substr(0, close + 1);
      if (close + 1 < authority.size() && authority[close + 1] == ':') {
        result.port = authority.substr(close + 2);
      }
      return result;
    }
  }

  const size_t colon = authority.rfind(':');
  if (colon != std::string_view::npos &&
      authority.find(':') == colon) {
    result.host = authority.substr(0, colon);
    result.port = authority.substr(colon + 1);
  } else {
    result.host = authority;
  }
  return result;
}

bool IsValidHost(std::string_view host) {
  if (host.empty() || host == ".") {
    return false;
  }

  if (host.front() == '[' && host.back() == ']') {
    // Conservative IPv6 literal check. Chromium performs the final validation.
    if (host.size() < 3) {
      return false;
    }
    for (size_t i = 1; i + 1 < host.size(); ++i) {
      const char c = host[i];
      if (std::isxdigit(static_cast<unsigned char>(c)) == 0 && c != ':' &&
          c != '.') {
        return false;
      }
    }
    return true;
  }

  if (host == "localhost") {
    return true;
  }

  bool has_dot = false;
  size_t label_start = 0;
  for (size_t i = 0; i <= host.size(); ++i) {
    if (i == host.size() || host[i] == '.') {
      const size_t length = i - label_start;
      if (length == 0 || length > 63) {
        return false;
      }
      if (host[label_start] == '-' || host[i - 1] == '-') {
        return false;
      }
      for (size_t j = label_start; j < i; ++j) {
        const char c = host[j];
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '-') {
          return false;
        }
      }
      if (i < host.size()) {
        has_dot = true;
      }
      label_start = i + 1;
    }
  }

  return has_dot;
}

bool IsValidHttpUrl(std::string_view url) {
  const std::string lower = Lower(url);
  if (!StartsWith(lower, "http://") && !StartsWith(lower, "https://")) {
    return false;
  }
  if (url.find(' ') != std::string_view::npos ||
      url.find('\\') != std::string_view::npos || HasControl(url)) {
    return false;
  }

  const HostPort host_port = SplitHostPort(AuthorityOf(url));
  if (host_port.has_userinfo || !IsValidHost(host_port.host)) {
    return false;
  }
  if (!host_port.port.empty() && !IsDigits(host_port.port)) {
    return false;
  }
  return true;
}

bool LooksLikeHostInput(std::string_view input) {
  if (input.empty() || input.find(' ') != std::string_view::npos ||
      input.find('\\') != std::string_view::npos ||
      input.find('@') != std::string_view::npos || HasControl(input)) {
    return false;
  }

  const std::string_view authority =
      input.substr(0, input.find_first_of("/?#"));
  const HostPort host_port = SplitHostPort(authority);
  if (!IsValidHost(host_port.host)) {
    return false;
  }
  if (!host_port.port.empty() && !IsDigits(host_port.port)) {
    return false;
  }
  return true;
}

bool IsHostPortShorthand(std::string_view input) {
  const size_t colon = input.find(':');
  if (colon == std::string_view::npos || colon == 0) {
    return false;
  }
  const std::string_view port = input.substr(colon + 1);
  if (port.empty() || !IsDigits(port) || port.size() > 5) {
    return false;
  }
  const std::string_view host = input.substr(0, colon);
  return IsValidHost(host);
}

std::string CanonicalNewTab() {
  return std::string(kNewTabUrl);
}

}  // namespace

std::string PercentEncodeQuery(std::string_view text) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(text.size() * 3);

  for (const char c : text) {
    const unsigned char byte = static_cast<unsigned char>(c);
    const bool unreserved =
        (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
        (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' ||
        byte == '_' || byte == '~';
    if (unreserved) {
      encoded.push_back(static_cast<char>(byte));
    } else {
      encoded.push_back('%');
      encoded.push_back(kHex[byte >> 4]);
      encoded.push_back(kHex[byte & 0x0F]);
    }
  }

  return encoded;
}

bool IsAllowedContentUrl(std::string_view url) {
  const std::string lower = Lower(url);
  if (StartsWith(lower, "nexus://newtab")) {
    const std::string_view remainder = url.substr(14);
    return remainder.empty() || remainder.front() == '/' ||
           remainder.front() == '?' || remainder.front() == '#';
  }
  return IsValidHttpUrl(url);
}

bool IsInternalAppUrl(std::string_view url) {
  const std::string lower = Lower(url);
  if (!StartsWith(lower, "nexus://app")) {
    return false;
  }
  const std::string_view remainder = url.substr(11);
  return remainder.empty() || remainder.front() == '/' ||
         remainder.front() == '?' || remainder.front() == '#';
}

bool IsInternalNewTabUrl(std::string_view url) {
  const std::string lower = Lower(url);
  if (!StartsWith(lower, "nexus://newtab")) {
    return false;
  }
  const std::string_view remainder = url.substr(14);
  return remainder.empty() || remainder.front() == '/' ||
         remainder.front() == '?' || remainder.front() == '#';
}

bool IsHttpsUrl(std::string_view url) {
  return StartsWithCI(url, "https://") && IsValidHttpUrl(url);
}

std::string OriginOfUrl(std::string_view url) {
  const std::string lower = Lower(url);
  std::string scheme;
  if (StartsWith(lower, "https://")) {
    scheme = "https";
  } else if (StartsWith(lower, "http://")) {
    scheme = "http";
  } else {
    return {};
  }

  const HostPort host_port = SplitHostPort(AuthorityOf(url));
  if (host_port.has_userinfo || !IsValidHost(host_port.host)) {
    return {};
  }

  std::string origin = scheme + "://" + Lower(host_port.host);
  if (!host_port.port.empty() && IsDigits(host_port.port)) {
    const bool default_port =
        (scheme == "http" && host_port.port == "80") ||
        (scheme == "https" && host_port.port == "443");
    if (!default_port) {
      origin += ":";
      origin += host_port.port;
    }
  }
  return origin;
}

std::string HostOfUrl(std::string_view url) {
  const std::string lower = Lower(url);
  if (!StartsWith(lower, "http://") && !StartsWith(lower, "https://")) {
    return {};
  }

  const HostPort host_port = SplitHostPort(AuthorityOf(url));
  if (host_port.has_userinfo || !IsValidHost(host_port.host)) {
    return {};
  }

  std::string host = Lower(host_port.host);
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  }
  return host;
}

NavigationTarget ResolveNavigationInput(std::string_view raw_input) {
  NavigationTarget result;
  const std::string input = Trim(raw_input);

  if (input.empty()) {
    result.error = "Enter an address or search term.";
    return result;
  }

  const std::string scheme = SchemeOf(input);
  if (!scheme.empty() && !IsHostPortShorthand(input)) {
    if (scheme == "http" || scheme == "https") {
      if (IsValidHttpUrl(input)) {
        result.url = input;
        result.allowed = true;
      } else {
        result.error = "That web address is not valid.";
      }
      return result;
    }
    if (scheme == "nexus") {
      if (IsAllowedContentUrl(input)) {
        result.url = CanonicalNewTab();
        result.allowed = true;
      } else {
        result.error =
            "Only the local NEXUS new-tab page may be opened from content.";
      }
      return result;
    }
    result.error =
        "For security, NEXUS opens only http, https, and its local new-tab "
        "page.";
    return result;
  }

  std::string candidate = input;
  if (StartsWith(candidate, "//")) {
    candidate.erase(0, 2);
  }

  if (LooksLikeHostInput(candidate)) {
    const std::string url = "https://" + candidate;
    if (IsValidHttpUrl(url)) {
      result.url = url;
      result.allowed = true;
      return result;
    }
  }

  result.url = std::string(kSearchPrefix) + PercentEncodeQuery(input);
  result.search = true;
  result.allowed = true;
  return result;
}

}  // namespace nexus::core
