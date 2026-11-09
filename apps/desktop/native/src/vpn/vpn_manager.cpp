#include "src/vpn/vpn_manager.h"

#include <chrono>
#include <sstream>

namespace nexus::vpn {
namespace {

std::string Trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

}  // namespace

VpnManager::VpnManager() = default;
VpnManager::~VpnManager() {
  Disconnect();
}

VpnTunnelInfo VpnManager::GetStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  VpnTunnelInfo info = current_tunnel_;
  if (info.status == VpnStatus::kConnected) {
    auto now = std::chrono::steady_clock::now();
    info.uptime_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now - connect_start_).count();
    // Simulate active tunnel byte transmission for UI telemetry
    info.bytes_received = 1048576 + info.uptime_seconds * 14200;
    info.bytes_sent = 524288 + info.uptime_seconds * 6800;
  }
  return info;
}

bool VpnManager::ParseConfig(const std::string& raw_content,
                             std::string* out_iface_ip,
                             std::string* out_endpoint,
                             std::string* out_dns) {
  if (raw_content.empty()) return false;

  std::istringstream stream(raw_content);
  std::string line;
  bool in_interface = false;
  bool in_peer = false;

  std::string iface_ip;
  std::string endpoint;
  std::string dns;

  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;

    if (line == "[Interface]") {
      in_interface = true;
      in_peer = false;
      continue;
    }
    if (line == "[Peer]") {
      in_peer = true;
      in_interface = false;
      continue;
    }

    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = Trim(line.substr(0, eq));
    std::string val = Trim(line.substr(eq + 1));

    if (in_interface) {
      if (key == "Address") iface_ip = val;
      else if (key == "DNS") dns = val;
    } else if (in_peer) {
      if (key == "Endpoint") endpoint = val;
    }
  }

  if (out_iface_ip) *out_iface_ip = iface_ip;
  if (out_endpoint) *out_endpoint = endpoint;
  if (out_dns) *out_dns = dns;

  return !endpoint.empty() || !iface_ip.empty();
}

bool VpnManager::Connect(const std::string& name, const std::string& config_content) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string iface_ip;
  std::string endpoint;
  std::string dns;

  if (!ParseConfig(config_content, &iface_ip, &endpoint, &dns)) {
    current_tunnel_.status = VpnStatus::kError;
    current_tunnel_.error_message = "Invalid WireGuard configuration syntax.";
    return false;
  }

  current_tunnel_.status = VpnStatus::kConnected;
  current_tunnel_.config_name = name;
  current_tunnel_.interface_ip = iface_ip.empty() ? "10.0.0.2/32" : iface_ip;
  current_tunnel_.endpoint = endpoint.empty() ? "vpn.nexus.internal:51820" : endpoint;
  current_tunnel_.dns = dns.empty() ? "1.1.1.1" : dns;
  current_tunnel_.error_message.clear();
  connect_start_ = std::chrono::steady_clock::now();
  current_tunnel_.connected_at =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();

  return true;
}

bool VpnManager::Disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);
  current_tunnel_.status = VpnStatus::kDisconnected;
  current_tunnel_.config_name.clear();
  current_tunnel_.uptime_seconds = 0;
  current_tunnel_.error_message.clear();
  return true;
}

void VpnManager::SetKillSwitch(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_tunnel_.kill_switch_enabled = enabled;
}

void VpnManager::SetDnsLeakProtection(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_tunnel_.dns_leak_protection = enabled;
}

}  // namespace nexus::vpn
