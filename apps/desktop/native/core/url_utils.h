#pragma once

#include <string>
#include <string_view>

namespace nexus::core {

struct NavigationTarget {
  std::string url;
  bool search = false;
  bool allowed = false;
  std::string error;
};

// Resolves address-bar input into an allowed content URL. Web input is always
// http(s); NEXUS only permits its own local new-tab page as an internal
// content URL. Everything else becomes a DuckDuckGo search.
NavigationTarget ResolveNavigationInput(std::string_view input);

// True for http(s) URLs and nexus://newtab. Never true for nexus://app,
// javascript:, data:, file:, blob:, chrome:, or any other scheme.
bool IsAllowedContentUrl(std::string_view url);

// True only for the trusted shell origin (nexus://app[/...]).
bool IsInternalAppUrl(std::string_view url);

// True only for the local new-tab content page (nexus://newtab[/...]).
bool IsInternalNewTabUrl(std::string_view url);

bool IsHttpsUrl(std::string_view url);

// Scheme + host + non-default port, lowercased. Empty for non-http(s) URLs.
std::string OriginOfUrl(std::string_view url);

// Lowercase host of an http(s) URL without IPv6 brackets. Empty otherwise.
std::string HostOfUrl(std::string_view url);

// Percent-encodes UTF-8 bytes for use in a query string.
std::string PercentEncodeQuery(std::string_view text);

}  // namespace nexus::core
