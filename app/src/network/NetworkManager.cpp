/**
 * Network Manager Implementation
 *
 * Uses wpa_cli (wpa_supplicant CLI) for WiFi management on Linux.
 */

#include "smarthub/network/NetworkManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <array>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <regex>
#include <fstream>

namespace smarthub {
namespace network {

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    if (m_initialized) return true;

    // Check if wpa_cli is available
    std::string version = executeCommand("wpa_cli -v 2>&1 | head -1");
    if (version.empty() || version.find("wpa_cli") == std::string::npos) {
        LOG_WARN("NetworkManager: wpa_cli not available");
        m_wifiAvailable = false;
        m_initialized = true;
        return false;
    }

    LOG_INFO("NetworkManager: %s", version.c_str());

    // Find WiFi interface (look for wlan*)
    std::string interfaces = executeCommand("ls /sys/class/net/ 2>/dev/null");
    std::istringstream iss(interfaces);
    std::string iface;
    while (iss >> iface) {
        if (iface.find("wlan") == 0) {
            m_wifiInterface = iface;
            break;
        }
    }

    if (m_wifiInterface.empty()) {
        LOG_WARN("NetworkManager: No WiFi interface found");
        m_wifiAvailable = false;
    } else {
        m_wifiAvailable = true;
        LOG_INFO("NetworkManager: WiFi interface: %s", m_wifiInterface.c_str());
    }

    m_running = true;
    m_initialized = true;

    // Get initial status
    updateStatus();

    return true;
}

void NetworkManager::shutdown() {
    m_running = false;

    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
    }
    m_workerThread.reset();

    m_initialized = false;
}

bool NetworkManager::isWifiAvailable() const {
    return m_wifiAvailable;
}

bool NetworkManager::setWifiEnabled(bool enabled) {
    if (!m_wifiAvailable) return false;

    std::string cmd = enabled
        ? "ip link set " + m_wifiInterface + " up"
        : "ip link set " + m_wifiInterface + " down";
    executeCommand(cmd);

    return true;
}

bool NetworkManager::isWifiEnabled() const {
    if (!m_wifiAvailable) return false;

    std::string result = executeCommand("ip link show " + m_wifiInterface);
    return result.find("UP") != std::string::npos;
}

void NetworkManager::startScan(ScanCallback callback) {
    if (!m_wifiAvailable || !m_running) {
        if (callback) {
            callback({});
        }
        return;
    }

    // Join previous thread if any
    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
    }

    // Start scan in background
    m_workerThread = std::make_unique<std::thread>(
        &NetworkManager::scanWorker, this, callback);
}

std::vector<WifiNetwork> NetworkManager::getScanResults() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scanResults;
}

void NetworkManager::connect(const std::string& ssid,
                             const std::string& password,
                             ConnectCallback callback) {
    if (!m_wifiAvailable || !m_running) {
        if (callback) {
            ConnectionResult result;
            result.success = false;
            result.error = "WiFi not available";
            callback(result);
        }
        return;
    }

    // Update status
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status.state = ConnectionState::Connecting;
        m_status.ssid = ssid;
    }

    if (m_statusCallback) {
        m_statusCallback(m_status);
    }

    // Join previous thread if any
    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
    }

    // Connect in background
    m_workerThread = std::make_unique<std::thread>(
        &NetworkManager::connectWorker, this, ssid, password, callback);
}

void NetworkManager::disconnect() {
    if (!m_wifiAvailable || m_wifiInterface.empty()) return;

    executeCommand("wpa_cli -i " + m_wifiInterface + " disconnect");

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status.state = ConnectionState::Disconnected;
        m_status.ssid.clear();
        m_status.ipAddress.clear();
    }

    if (m_statusCallback) {
        m_statusCallback(m_status);
    }
}

bool NetworkManager::forgetNetwork(const std::string& ssid) {
    if (!m_wifiAvailable) return false;

    // List networks and find the one with matching SSID
    std::string listOutput = executeCommand(
        "wpa_cli -i " + m_wifiInterface + " list_networks");

    std::istringstream iss(listOutput);
    std::string line;
    std::getline(iss, line);  // Skip header

    while (std::getline(iss, line)) {
        // Format: network_id / ssid / bssid / flags
        std::istringstream lineStream(line);
        std::string networkId, networkSsid;
        lineStream >> networkId >> networkSsid;

        if (networkSsid == ssid) {
            executeCommand("wpa_cli -i " + m_wifiInterface +
                          " remove_network " + networkId);
            executeCommand("wpa_cli -i " + m_wifiInterface + " save_config");
            return true;
        }
    }

    return false;
}

NetworkStatus NetworkManager::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

std::vector<std::string> NetworkManager::getSavedNetworks() const {
    std::vector<std::string> networks;

    if (!m_wifiAvailable) return networks;

    std::string output = executeCommand(
        "wpa_cli -i " + m_wifiInterface + " list_networks");

    std::istringstream iss(output);
    std::string line;
    std::getline(iss, line);  // Skip header

    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string networkId, ssid;
        lineStream >> networkId >> ssid;
        if (!ssid.empty()) {
            networks.push_back(ssid);
        }
    }

    return networks;
}

void NetworkManager::setStatusCallback(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCallback = callback;
}

int NetworkManager::signalToIconIndex(int signalStrength) {
    // wpa_cli returns signal in dBm, convert to index
    // Typical range: -90 dBm (weak) to -30 dBm (strong)
    if (signalStrength >= -50) return 4;  // Excellent
    if (signalStrength >= -60) return 3;  // Good
    if (signalStrength >= -70) return 2;  // Fair
    if (signalStrength >= -80) return 1;  // Weak
    return 0;  // Very weak
}

int NetworkManager::dbmToPercent(int dbm) {
    // Typical range: -90 dBm (weak) to -30 dBm (strong)
    if (dbm <= -90) return 0;
    if (dbm >= -30) return 100;
    return (dbm + 90) * 100 / 60;
}

std::string NetworkManager::executeCommand(const std::string& command) const {
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        LOG_ERROR("NetworkManager: Failed to execute: %s", command.c_str());
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    if (status != 0) {
        LOG_DEBUG("NetworkManager: Command returned %d: %s", status, command.c_str());
    }

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

std::vector<WifiNetwork> NetworkManager::parseScanOutput(const std::string& output) {
    std::vector<WifiNetwork> networks;

    // wpa_cli scan_results format:
    // bssid / frequency / signal level / flags / ssid
    std::istringstream iss(output);
    std::string line;

    // Skip header line
    std::getline(iss, line);

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Parse tab-separated fields
        std::vector<std::string> fields;
        std::string field;
        std::istringstream lineStream(line);

        while (std::getline(lineStream, field, '\t')) {
            fields.push_back(field);
        }

        if (fields.size() >= 5) {
            WifiNetwork net;
            net.bssid = fields[0];

            try {
                net.frequency = std::stoi(fields[1]);
            } catch (...) {
                net.frequency = 2400;
            }

            try {
                net.signalStrength = std::stoi(fields[2]);
            } catch (...) {
                net.signalStrength = -100;
            }

            // Parse flags for security
            std::string flags = fields[3];
            net.secured = (flags.find("WPA") != std::string::npos ||
                          flags.find("WEP") != std::string::npos);

            if (flags.find("WPA2") != std::string::npos) {
                net.security = "WPA2";
            } else if (flags.find("WPA") != std::string::npos) {
                net.security = "WPA";
            } else if (flags.find("WEP") != std::string::npos) {
                net.security = "WEP";
            } else {
                net.security = "";
            }

            net.ssid = fields[4];

            // Skip networks with empty SSID (hidden networks)
            if (net.ssid.empty()) continue;

            net.connected = false;
            networks.push_back(net);
        }
    }

    // Sort by signal strength (strongest first, remember dBm is negative)
    std::sort(networks.begin(), networks.end(),
              [](const WifiNetwork& a, const WifiNetwork& b) {
                  return a.signalStrength > b.signalStrength;
              });

    // Remove duplicates (same SSID), keeping strongest signal
    auto it = std::unique(networks.begin(), networks.end(),
                          [](const WifiNetwork& a, const WifiNetwork& b) {
                              return a.ssid == b.ssid;
                          });
    networks.erase(it, networks.end());

    return networks;
}

void NetworkManager::updateStatus() {
    std::string output = executeCommand(
        "wpa_cli -i " + m_wifiInterface + " status");

    NetworkStatus status;
    status.state = ConnectionState::Disconnected;

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (key == "wpa_state") {
            if (value == "COMPLETED") {
                status.state = ConnectionState::Connected;
            } else if (value == "SCANNING") {
                status.state = ConnectionState::Scanning;
            } else if (value == "ASSOCIATING" || value == "ASSOCIATED" ||
                       value == "4WAY_HANDSHAKE" || value == "GROUP_HANDSHAKE") {
                status.state = ConnectionState::Connecting;
            } else if (value == "DISCONNECTED" || value == "INACTIVE") {
                status.state = ConnectionState::Disconnected;
            }
        } else if (key == "ssid") {
            status.ssid = value;
        } else if (key == "ip_address") {
            status.ipAddress = value;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status = status;
    }
}

NetworkStatus NetworkManager::parseStatusOutput(const std::string& output) {
    // This method is kept for compatibility but updateStatus() is preferred
    NetworkStatus status;
    status.state = ConnectionState::Disconnected;

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (key == "wpa_state" && value == "COMPLETED") {
            status.state = ConnectionState::Connected;
        } else if (key == "ssid") {
            status.ssid = value;
        } else if (key == "ip_address") {
            status.ipAddress = value;
        }
    }

    return status;
}

void NetworkManager::scanWorker(ScanCallback callback) {
    LOG_DEBUG("NetworkManager: Starting WiFi scan...");

    // Request a scan
    executeCommand("wpa_cli -i " + m_wifiInterface + " scan");

    // Wait for scan to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // Get scan results
    std::string output = executeCommand(
        "wpa_cli -i " + m_wifiInterface + " scan_results");

    std::vector<WifiNetwork> networks = parseScanOutput(output);

    // Mark currently connected network
    updateStatus();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& net : networks) {
            if (m_status.state == ConnectionState::Connected &&
                net.ssid == m_status.ssid) {
                net.connected = true;
            }
        }
        m_scanResults = networks;
    }

    LOG_DEBUG("NetworkManager: Scan found %zu networks", networks.size());

    if (callback) {
        callback(networks);
    }
}

void NetworkManager::connectWorker(const std::string& ssid,
                                   const std::string& password,
                                   ConnectCallback callback) {
    ConnectionResult result;

    LOG_INFO("NetworkManager: Connecting to '%s'...", ssid.c_str());

    // Check if network already exists in saved networks
    std::string listOutput = executeCommand(
        "wpa_cli -i " + m_wifiInterface + " list_networks");

    std::string networkId;
    std::istringstream iss(listOutput);
    std::string line;
    std::getline(iss, line);  // Skip header

    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string id, savedSsid;
        lineStream >> id >> savedSsid;

        if (savedSsid == ssid) {
            networkId = id;
            break;
        }
    }

    // If network doesn't exist, create it
    if (networkId.empty()) {
        std::string addOutput = executeCommand(
            "wpa_cli -i " + m_wifiInterface + " add_network");

        // Parse network ID from output (just a number)
        try {
            networkId = addOutput;
            // Trim whitespace
            networkId.erase(0, networkId.find_first_not_of(" \t\n\r"));
            networkId.erase(networkId.find_last_not_of(" \t\n\r") + 1);
        } catch (...) {
            result.success = false;
            result.error = "Failed to create network";
            if (callback) callback(result);
            return;
        }

        // Set SSID (must be quoted)
        executeCommand("wpa_cli -i " + m_wifiInterface +
                      " set_network " + networkId + " ssid '\"" + ssid + "\"'");

        // Set password or key_mgmt for open networks
        if (password.empty()) {
            executeCommand("wpa_cli -i " + m_wifiInterface +
                          " set_network " + networkId + " key_mgmt NONE");
        } else {
            executeCommand("wpa_cli -i " + m_wifiInterface +
                          " set_network " + networkId + " psk '\"" + password + "\"'");
        }
    }

    // Enable the network
    executeCommand("wpa_cli -i " + m_wifiInterface +
                  " enable_network " + networkId);

    // Select the network (this triggers connection)
    std::string selectResult = executeCommand(
        "wpa_cli -i " + m_wifiInterface + " select_network " + networkId);

    if (selectResult.find("OK") == std::string::npos) {
        result.success = false;
        result.error = "Failed to select network";

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_status.state = ConnectionState::Failed;
            m_status.error = result.error;
        }

        if (callback) callback(result);
        return;
    }

    // Wait for connection with timeout
    int attempts = 0;
    const int maxAttempts = 30;  // 15 seconds total

    while (attempts < maxAttempts) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        updateStatus();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_status.state == ConnectionState::Connected) {
                break;
            }
        }

        attempts++;
    }

    // Check final status
    updateStatus();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state == ConnectionState::Connected) {
            result.success = true;
            result.ipAddress = m_status.ipAddress;

            // Save config so it persists across reboots
            executeCommand("wpa_cli -i " + m_wifiInterface + " save_config");

            LOG_INFO("NetworkManager: Connected to '%s', IP: %s",
                     ssid.c_str(), result.ipAddress.c_str());
        } else {
            result.success = false;

            // Determine error reason
            std::string statusOutput = executeCommand(
                "wpa_cli -i " + m_wifiInterface + " status");

            if (statusOutput.find("INACTIVE") != std::string::npos ||
                statusOutput.find("DISCONNECTED") != std::string::npos) {
                result.error = "Incorrect password or network unavailable";
            } else {
                result.error = "Connection timed out";
            }

            m_status.state = ConnectionState::Failed;
            m_status.error = result.error;

            LOG_WARN("NetworkManager: Failed to connect to '%s': %s",
                     ssid.c_str(), result.error.c_str());
        }
    }

    if (m_statusCallback) {
        m_statusCallback(m_status);
    }

    if (callback) {
        callback(result);
    }
}

} // namespace network
} // namespace smarthub
