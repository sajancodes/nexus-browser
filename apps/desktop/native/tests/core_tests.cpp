#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/data_paths.h"
#include "core/url_utils.h"
#include "db/database.h"
#include "privacy/filter_engine.h"
#include "vpn/vpn_manager.h"

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
    CHECK(result.url == "https://www.google.com/search?q=privacy%20browser");
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
        ParseLaunchOptions({L"nexus.exe", L"--url=https://example.com"}, L"");
    CHECK(options.error.empty());
    CHECK(options.initial_url == "https://example.com");
  }
  {
    const auto options =
        ParseLaunchOptions({L"nexus.exe", L"https://example.org"}, L"");
    CHECK(options.error.empty());
    CHECK(options.initial_url == "https://example.org");
  }
  {
    const auto options =
        ParseLaunchOptions({L"nexus.exe", L"--smoke-test"}, L"");
    CHECK(options.smoke_test);
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

void TestFilterEngine() {
  using nexus::privacy::FilterEngine;
  FilterEngine engine;

  // Tracker domains
  CHECK(engine.ShouldBlock("https://www.google-analytics.com/analytics.js", "example.com"));
  CHECK(engine.ShouldBlock("https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js", "example.com"));
  CHECK(engine.ShouldBlock("https://connect.facebook.net/en_US/fbevents.js", "example.com"));
  CHECK(engine.ShouldBlock("https://c.amazon-adsystem.com/aax2/apstag.js", "example.com"));
  CHECK(engine.ShouldBlock("https://pixel.rubiconproject.com/tap.php", "example.com"));
  CHECK(engine.ShouldBlock("https://secure.quantserve.com/quant.js", "example.com"));
  CHECK(engine.ShouldBlock("https://cdn.taboola.com/libtrc/loader.js", "example.com"));
  CHECK(engine.ShouldBlock("https://outbrain.com/widget", "example.com"));

  // Common tracking substrings
  CHECK(engine.ShouldBlock("https://example.com/api/telemetry/report", "example.com"));
  CHECK(engine.ShouldBlock("https://example.com/collect/beacon?user=1", "example.com"));
  CHECK(engine.ShouldBlock("https://example.com/adserver/banner.png", "example.com"));

  // Legitimate requests that should pass
  CHECK(!engine.ShouldBlock("https://example.com/index.html", "example.com"));
  CHECK(!engine.ShouldBlock("https://en.wikipedia.org/wiki/Main_Page", "en.wikipedia.org"));
  CHECK(!engine.ShouldBlock("https://duckduckgo.com/", "duckduckgo.com"));
  CHECK(!engine.ShouldBlock("https://github.com/sajancodes/nexus-browser", "github.com"));
}

void TestVpnConfigParsing() {
  using nexus::vpn::VpnManager;

  const std::string valid_conf =
      "[Interface]\n"
      "PrivateKey = aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=\n"
      "Address = 10.0.0.2/32\n"
      "DNS = 1.1.1.1, 1.0.0.1\n"
      "\n"
      "[Peer]\n"
      "PublicKey = bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb=\n"
      "Endpoint = 198.51.100.1:51820\n"
      "AllowedIPs = 0.0.0.0/0\n";

  std::string iface, endpoint, dns;
  bool ok = VpnManager::ParseConfig(valid_conf, &iface, &endpoint, &dns);
  CHECK(ok);
  CHECK(iface == "10.0.0.2/32");
  CHECK(dns == "1.1.1.1, 1.0.0.1");
  CHECK(endpoint == "198.51.100.1:51820");

  const std::string invalid_conf = "This is not a wireguard configuration";
  bool invalid_ok = VpnManager::ParseConfig(invalid_conf, &iface, &endpoint, &dns);
  CHECK(!invalid_ok);
}

void TestDatabaseOperations() {
  using nexus::db::Database;
  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() /
      (L"nexus-db-test-" +
       std::to_wstring(static_cast<unsigned long long>(GetTickCount64())));
  std::filesystem::create_directories(temp_dir);

  const std::filesystem::path db_file = temp_dir / L"test_nexus.db";
  {
    Database db;
    CHECK(db.Open(db_file));
    CHECK(db.IsOpen());

    // History operations
    CHECK(db.AddHistory("https://example.com/test", "Test Page"));
    auto recent = db.GetHistory(10);
    CHECK(!recent.empty());
    CHECK(recent[0].url == "https://example.com/test");
    CHECK(recent[0].title == "Test Page");

    auto ac = db.SearchAutocomplete("example", 5);
    CHECK(!ac.empty());
    CHECK(ac[0].url == "https://example.com/test");

    // Bookmark operations
    CHECK(!db.IsBookmarked("https://example.com/bookmark"));
    CHECK(db.AddBookmark("https://example.com/bookmark", "Saved Bookmark"));
    CHECK(db.IsBookmarked("https://example.com/bookmark"));
    auto bookmarks = db.GetBookmarks();
    CHECK(bookmarks.size() == 1);
    CHECK(bookmarks[0].title == "Saved Bookmark");
    CHECK(db.RemoveBookmark("https://example.com/bookmark"));
    CHECK(!db.IsBookmarked("https://example.com/bookmark"));

    // Settings operations
    CHECK(db.SetSetting("test_key", "test_value_123"));
    CHECK(db.GetSetting("test_key") == "test_value_123");
    CHECK(db.GetSetting("non_existent", "default_val") == "default_val");

    db.Close();
  }

  // Clean up
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
  CHECK(!ec);
}

}  // namespace

int main() {
  TestNavigationResolution();
  TestUrlPolicy();
  TestDataPaths();
  TestFilterEngine();
  TestVpnConfigParsing();
  TestDatabaseOperations();

  if (g_failures == 0) {
    std::cout << "nexus_core_tests: all checks passed\n";
    return 0;
  }

  std::cerr << "nexus_core_tests: " << g_failures << " check(s) failed\n";
  return 1;
}
