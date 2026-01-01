/**
 * Network Manager Implementation
 *
 * Uses nmcli (NetworkManager CLI) for WiFi management on Linux.
 */

#include "smarthub/network/NetworkManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <array>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <regex>

namespace smarthub {
namespace network {

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    if (m_initialized) return true;

    // Check if nmcli is available
    std::string version = executeCommand("nmcli --version 2>/dev/null");
    if (version.empty() || version.find("nmcli") == std::string::npos) {
        LOG_WARN("NetworkManager: nmcli not available");
        m_wifiAvailable = false;
        m_initialized = true;
        return false;
    }

    LOG_INFO("NetworkManager: %s", version.c_str());

    // Check for WiFi device
    std::string devices = executeCommand("nmcli device status 2>/dev/null");
    if (devices.find("wifi") != std::string::npos) {
        m_wifiAvailable = true;

        // Find WiFi interface name
        std::istringstream iss(devices);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("wifi") != std::string::npos) {
                std::istringstream lineStream(line);
                lineStream >> m_wifiInterface;
                break;
            }
        }

        LOG_INFO("NetworkManager: WiFi interface: %s", m_wifiInterface.c_str());
    } else {
        LOG_WARN("NetworkManager: No WiFi device found");
        m_wifiAvailable = false;
    }

    m_running = true;
    m_initialized = true;

    // Get initial status
    m_status = parseStatusOutput(executeCommand("nmcli -t -f active,ssid,signal dev wifi 2>/dev/null"));

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

    std::string cmd = enabled ? "nmcli radio wifi on" : "nmcli radio wifi off";
    std::string result = executeCommand(cmd);

    return result.find("Error") == std::string::npos;
}

bool NetworkManager::isWifiEnabled() const {
    if (!m_wifiAvailable) return false;

    std::string result = executeCommand("nmcli radio wifi");
    return result.find("enabled") != std::string::npos;
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

    executeCommand("nmcli device disconnect " + m_wifiInterface);

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

    // Get connection UUID for this SSID
    std::string listCmd = "nmcli -t -f NAME,UUID connection show";
    std::string output = executeCommand(listCmd);

    std::istringstream iss(output);
    std::string line;
    std::string uuid;

    while (std::getline(iss, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string name = line.substr(0, pos);
            if (name == ssid) {
                uuid = line.substr(pos + 1);
                break;
            }
        }
    }

    if (uuid.empty()) return false;

    std::string delCmd = "nmcli connection delete uuid " + uuid;
    std::string result = executeCommand(delCmd);

    return result.find("successfully") != std::string::npos;
}

NetworkStatus NetworkManager::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

std::vector<std::string> NetworkManager::getSavedNetworks() const {
    std::vector<std::string> networks;

    if (!m_wifiAvailable) return networks;

    std::string output = executeCommand("nmcli -t -f NAME,TYPE connection show");
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find("802-11-wireless") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                networks.push_back(line.substr(0, pos));
            }
        }
    }

    return networks;
}

void NetworkManager::setStatusCallback(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCallback = callback;
}

int NetworkManager::signalToIconIndex(int signalStrength) {
    if (signalStrength >= 80) return 4;  // Excellent
    if (signalStrength >= 60) return 3;  // Good
    if (signalStrength >= 40) return 2;  // Fair
    if (signalStrength >= 20) return 1;  // Weak
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

    // nmcli -t -f SSID,BSSID,SIGNAL,SECURITY,FREQ,ACTIVE dev wifi list
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Parse colon-separated fields
        std::vector<std::string> fields;
        std::string field;
        bool escaped = false;

        for (char c : line) {
            if (escaped) {
                field += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == ':') {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(field);

        if (fields.size() >= 5) {
            WifiNetwork net;
            net.ssid = fields[0];
            net.bssid = fields[1];

            // Skip networks with empty SSID (hidden networks)
            if (net.ssid.empty()) continue;

            try {
                net.signalStrength = std::stoi(fields[2]);
            } catch (...) {
                net.signalStrength = 0;
            }

            net.security = fields[3];
            net.secured = !net.security.empty() && net.security != "--";

            try {
                net.frequency = std::stoi(fields[4]);
            } catch (...) {
                net.frequency = 2400;
            }

            if (fields.size() > 5) {
                net.connected = (fields[5] == "yes");
            }

            networks.push_back(net);
        }
    }

    // Sort by signal strength (strongest first)
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

NetworkStatus NetworkManager::parseStatusOutput(const std::string& output) {
    NetworkStatus status;
    status.state = ConnectionState::Disconnected;

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        // Format: ACTIVE:SSID:SIGNAL
        std::vector<std::string> fields;
        std::string field;

        for (char c : line) {
            if (c == ':') {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(field);

        if (fields.size() >= 3 && fields[0] == "yes") {
            status.state = ConnectionState::Connected;
            status.ssid = fields[1];
            try {
                status.signalStrength = std::stoi(fields[2]);
            } catch (...) {
                status.signalStrength = 0;
            }
            break;
        }
    }

    // Get IP address if connected
    if (status.state == ConnectionState::Connected && !m_wifiInterface.empty()) {
        std::string ipOutput = executeCommand(
            "nmcli -t -f IP4.ADDRESS dev show " + m_wifiInterface + " 2>/dev/null | head -1");
        if (!ipOutput.empty()) {
            size_t pos = ipOutput.find(':');
            if (pos != std::string::npos) {
                std::string ip = ipOutput.substr(pos + 1);
                // Remove CIDR notation
                size_t slashPos = ip.find('/');
                if (slashPos != std::string::npos) {
                    ip = ip.substr(0, slashPos);
                }
                status.ipAddress = ip;
            }
        }
    }

    return status;
}

void NetworkManager::scanWorker(ScanCallback callback) {
    LOG_DEBUG("NetworkManager: Starting WiFi scan...");

    // Request a fresh scan
    executeCommand("nmcli device wifi rescan 2>/dev/null");

    // Wait a moment for scan to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Get scan results
    std::string output = executeCommand(
        "nmcli -t -f SSID,BSSID,SIGNAL,SECURITY,FREQ,ACTIVE dev wifi list 2>/dev/null");

    std::vector<WifiNetwork> networks = parseScanOutput(output);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
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

    std::string cmd;
    if (password.empty()) {
        // Open network
        cmd = "nmcli device wifi connect \"" + ssid + "\" 2>&1";
    } else {
        // Secured network
        cmd = "nmcli device wifi connect \"" + ssid + "\" password \"" + password + "\" 2>&1";
    }

    std::string output = executeCommand(cmd);

    if (output.find("successfully") != std::string::npos) {
        result.success = true;

        // Get IP address
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        NetworkStatus status = parseStatusOutput(
            executeCommand("nmcli -t -f active,ssid,signal dev wifi 2>/dev/null"));

        if (!m_wifiInterface.empty()) {
            std::string ipOutput = executeCommand(
                "nmcli -t -f IP4.ADDRESS dev show " + m_wifiInterface + " 2>/dev/null | head -1");
            size_t pos = ipOutput.find(':');
            if (pos != std::string::npos) {
                std::string ip = ipOutput.substr(pos + 1);
                size_t slashPos = ip.find('/');
                if (slashPos != std::string::npos) {
                    ip = ip.substr(0, slashPos);
                }
                result.ipAddress = ip;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_status.state = ConnectionState::Connected;
            m_status.ssid = ssid;
            m_status.ipAddress = result.ipAddress;
            m_status.error.clear();
        }

        LOG_INFO("NetworkManager: Connected to '%s', IP: %s",
                 ssid.c_str(), result.ipAddress.c_str());
    } else {
        result.success = false;

        // Parse error message
        if (output.find("Secrets were required") != std::string::npos ||
            output.find("password") != std::string::npos) {
            result.error = "Incorrect password";
        } else if (output.find("No network with SSID") != std::string::npos) {
            result.error = "Network not found";
        } else if (output.find("timed out") != std::string::npos) {
            result.error = "Connection timed out";
        } else {
            result.error = "Connection failed";
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_status.state = ConnectionState::Failed;
            m_status.error = result.error;
        }

        LOG_WARN("NetworkManager: Failed to connect to '%s': %s",
                 ssid.c_str(), result.error.c_str());
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
