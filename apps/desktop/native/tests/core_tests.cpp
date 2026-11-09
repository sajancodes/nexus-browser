#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/data_paths.h"
#include "core/url_utils.h"

namespace {

int g_failures = 0;

#define CHECK(condition)                                                    \
  do {                                                                      \
    if (!(condition)) {                                                     \
      std::cerr << __FILE__ << ":" << __LINE__                              \
                << " CHECK failed: " #condition << "\n";                    \
      ++g_failures;                                                        \
    }                                                                       \
  } while (false)

void TestNavigationResolution() {
  using nexus::core::ResolveNavigationInput;

  {
    const auto result = ResolveNavigationInput("example.com");
    CHECK(result.allowed);
    CHECK(!result.search);
    CHECK(result.url == "https://example.com");
  }
  {
    const auto result = ResolveNavigationInput("  https://example.com/a?b=1  ");
    CHECK(result.allowed);
    CHECK(result.url == "https://example.com/a?b=1");
  }
  {
    const auto result = ResolveNavigationInput("localhost:8080");
    CHECK(result.allowed);
    CHECK(result.url == "https://localhost:8080");
  }
  {
    const auto result = ResolveNavigationInput("127.0.0.1:3000");
    CHECK(result.allowed);
    CHECK(result.url == "https://127.0.0.1:3000");
  }
  {
    const auto result = ResolveNavigationInput("[::1]:8080");
    CHECK(result.allowed);
    CHECK(result.url == "https://[::1]:8080");
  }
  {
    const auto result = ResolveNavigationInput("//example.com/path");
    CHECK(result.allowed);
    CHECK(result.url == "https://example.com/path");
  }
  {
    const auto result = ResolveNavigationInput("example.com/path?q=1");
    CHECK(result.allowed);
    CHECK(!result.search);
    CHECK(result.url == "https://example.com/path?q=1");
  }
  {
    const auto result = ResolveNavigationInput("privacy browser");
    CHECK(result.allowed);
    CHECK(result.search);
    CHECK(result.url == "https://duckduckgo.com/?q=privacy%20browser");
  }
  {
    const auto result = ResolveNavigationInput("nexus://newtab/?focus=1");
    CHECK(result.allowed);
    CHECK(result.url == "nexus://newtab/");
  }
  {
    const auto result = ResolveNavigationInput("javascript:alert(1)");
    CHECK(!result.allowed);
    CHECK(result.error.empty() == false);
  }
  {
    const auto result = ResolveNavigationInput("file:///C:/Windows");
    CHECK(!result.allowed);
  }
  {
    const auto result = ResolveNavigationInput("nexus://app/");
    CHECK(!result.allowed);
  }
  {
    const auto result = ResolveNavigationInput("http://");
    CHECK(!result.allowed);
  }
  {
    const auto result = ResolveNavigationInput("user:pass@example.com");
    CHECK(!result.allowed);
  }
  {
    const auto result = ResolveNavigationInput("");
    CHECK(!result.allowed);
  }
}

void TestUrlPolicy() {
  using nexus::core::HostOfUrl;
  using nexus::core::IsAllowedContentUrl;
  using nexus::core::IsInternalAppUrl;
  using nexus::core::IsInternalNewTabUrl;
  using nexus::core::OriginOfUrl;
  using nexus::core::PercentEncodeQuery;

  CHECK(IsAllowedContentUrl("http://example.com/"));
  CHECK(IsAllowedContentUrl("https://example.com/path"));
  CHECK(IsAllowedContentUrl("nexus://newtab/"));
  CHECK(!IsAllowedContentUrl("nexus://app/"));
  CHECK(!IsAllowedContentUrl("nexus://newtab.evil/"));
  CHECK(!IsAllowedContentUrl("data:text/html,hello"));
  CHECK(!IsAllowedContentUrl("file:///C:/Windows/win.ini"));
  CHECK(!IsAllowedContentUrl("javascript:alert(1)"));
  CHECK(!IsAllowedContentUrl("https://user:pass@example.com/"));

  CHECK(IsInternalAppUrl("nexus://app/"));
  CHECK(IsInternalAppUrl("nexus://app/index.html"));
  CHECK(IsInternalAppUrl("NEXUS://APP/"));
  CHECK(!IsInternalAppUrl("nexus://app.evil/"));
  CHECK(!IsInternalAppUrl("https://example.com/"));

  CHECK(IsInternalNewTabUrl("nexus://newtab/"));
  CHECK(IsInternalNewTabUrl("nexus://newtab/index.html?x=1"));
  CHECK(!IsInternalNewTabUrl("nexus://newtab.evil/"));

  CHECK(OriginOfUrl("HTTPS://Example.COM:443/a") == "https://example.com");
  CHECK(OriginOfUrl("http://example.com:8080/a") ==
        "http://example.com:8080");
  CHECK(OriginOfUrl("https://example.com:80/") == "https://example.com:80");
  CHECK(OriginOfUrl("file:///C:/") == "");

  CHECK(HostOfUrl("https://Sub.Example.com:8443/x") == "sub.example.com");
  CHECK(HostOfUrl("https://[::1]:8443/") == "::1");
  CHECK(HostOfUrl("nexus://app/") == "");

  CHECK(PercentEncodeQuery("a b&c") == "a%20b%26c");
  CHECK(PercentEncodeQuery("safe-._~") == "safe-._~");
  CHECK(PercentEncodeQuery("\xE2\x9C\x93") == "%E2%9C%93");
}

void TestDataPaths() {
  using nexus::core::EnsureDataLayout;
  using nexus::core::ParseLaunchOptions;

  {
    const auto options =
        ParseLaunchOptions({L"nexus.exe", L"--user-data-dir=C:\\nexus-data\\a"},
                           L"D:\\ignored");
    CHECK(options.error.empty());
    CHECK(options.data_root.filename() == L"a");
  }
  {
    const auto options = ParseLaunchOptions({L"nexus.exe"}, L"C:\\nexus-env");
    CHECK(options.error.empty());
    CHECK(options.data_root.filename() == L"nexus-env");
  }
  {
    const auto options =
        ParseLaunchOptions({L"nexus.exe", L"--no-sandbox"}, L"");
    CHECK(options.no_sandbox_requested);
    CHECK(!options.error.empty());
  }
  {
    const auto options =
        ParseLaunchOptions({L"nexus.exe", L"--user-data-dir=C:\\"}, L"");
    CHECK(!options.error.empty());
  }

  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      (L"nexus-core-test-" +
       std::to_wstring(static_cast<unsigned long long>(GetTickCount64())));
  std::string error;
  CHECK(EnsureDataLayout(root, &error));
  CHECK(std::filesystem::is_directory(root));
  CHECK(std::filesystem::is_directory(root / L"cef"));
  CHECK(std::filesystem::is_directory(root / L"cef" / L"cache"));
  CHECK(std::filesystem::is_directory(root / L"logs"));
  CHECK(std::filesystem::is_directory(root / L"profile"));
  std::error_code cleanup_error;
  std::filesystem::remove_all(root, cleanup_error);
  CHECK(!cleanup_error);
}

}  // namespace

int main() {
  TestNavigationResolution();
  TestUrlPolicy();
  TestDataPaths();

  if (g_failures == 0) {
    std::cout << "nexus_core_tests: all checks passed\n";
    return 0;
  }

  std::cerr << "nexus_core_tests: " << g_failures << " check(s) failed\n";
  return 1;
}
