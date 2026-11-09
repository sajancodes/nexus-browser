#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::vpn {

enum class VpnStatus {
  kDisconnected = 0,
  kConnecting = 1,
  kConnected = 2,
  kDisconnecting = 3,
  kError = 4,
};

struct VpnTunnelInfo {
  VpnStatus status = VpnStatus::kDisconnected;
  std::string config_name;
  std::string endpoint;
  std::string interface_ip;
  std::string dns;
  int64_t connected_at = 0;
  int64_t uptime_seconds = 0;
  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;
  bool kill_switch_enabled = true;
  bool dns_leak_protection = true;
  std::string error_message;
};

class VpnManager {
 public:
  VpnManager();
  ~VpnManager();

  VpnTunnelInfo GetStatus() const;
  bool Connect(const std::string& name, const std::string& config_content);
  bool Disconnect();
  void SetKillSwitch(bool enabled);
  void SetDnsLeakProtection(bool enabled);

  // WireGuard config parser
  static bool ParseConfig(const std::string& raw_content,
                          std::string* out_iface_ip,
                          std::string* out_endpoint,
                          std::string* out_dns);

 private:
  mutable std::mutex mutex_;
  VpnTunnelInfo current_tunnel_;
  std::chrono::steady_clock::time_point connect_start_;
};

}  // namespace nexus::vpn
