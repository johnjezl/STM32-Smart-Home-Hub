/**
 * Network Manager Implementation
 *
 * Uses wpa_cli (wpa_supplicant CLI) for WiFi management on Linux. Every command is run with an
 * argument list rather than through a shell, so SSIDs and passwords can't be taken as commands.
 */

#include "smarthub/network/NetworkManager.hpp"
#include "smarthub/core/Logger.hpp"
#include "smarthub/core/Process.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace smarthub {
namespace network {

namespace {

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    return text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}

bool isNetworkId(const std::string& text) {
    return !text.empty() && text.size() < 10 &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); });
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

constexpr size_t kMaxSsidBytes = 32;

}  // namespace

NetworkManager::NetworkManager() : NetworkManager(NetworkManagerOptions{}) {}

NetworkManager::NetworkManager(NetworkManagerOptions options) : m_options(std::move(options)) {}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    if (m_initialized) return true;

    // Check if wpa_cli is available
    const auto versionRun = core::runProcess({m_options.wpaCli, "-v"}, true);
    const std::string version = trim(versionRun.output.substr(0, versionRun.output.find('\n')));
    if (version.find("wpa_cli") == std::string::npos) {
        LOG_WARN("NetworkManager: wpa_cli not available");
        m_wifiAvailable = false;
        m_initialized = true;
        return false;
    }

    LOG_INFO("NetworkManager: %s", version.c_str());

    // The first WiFi interface (wlan*) by name
    std::vector<std::string> interfaces;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(m_options.sysClassNet, error)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("wlan", 0) == 0) {
            interfaces.push_back(name);
        }
    }
    std::sort(interfaces.begin(), interfaces.end());
    if (!interfaces.empty()) {
        m_wifiInterface = interfaces.front();
    }

    if (m_wifiInterface.empty()) {
        LOG_WARN("NetworkManager: No WiFi interface found");
        m_wifiAvailable = false;
    } else {
        m_wifiAvailable = true;
        LOG_INFO("NetworkManager: WiFi interface: %s", m_wifiInterface.c_str());
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopping = false;
    }
    m_running = true;
    m_initialized = true;

    // Get initial status
    if (m_wifiAvailable) {
        updateStatus();
    }

    return true;
}

void NetworkManager::shutdown() {
    m_running = false;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopping = true;
        // Tasks that haven't started are dropped, and so are their callbacks
        m_queue.clear();
    }
    m_queueChanged.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_initialized = false;
}

bool NetworkManager::isWifiAvailable() const {
    return m_wifiAvailable;
}

bool NetworkManager::setWifiEnabled(bool enabled) {
    if (!m_wifiAvailable) return false;

    const auto result =
        core::runProcess({m_options.ip, "link", "set", m_wifiInterface, enabled ? "up" : "down"});
    return result.exitCode == 0;
}

bool NetworkManager::isWifiEnabled() const {
    if (!m_wifiAvailable) return false;

    std::ifstream flagsFile(m_options.sysClassNet + "/" + m_wifiInterface + "/flags");
    std::string flags;
    if (!(flagsFile >> flags)) {
        return false;
    }
    try {
        return (std::stoul(flags, nullptr, 16) & 0x1) != 0;  // IFF_UP
    } catch (...) {
        return false;
    }
}

void NetworkManager::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_stopping) {
            return;
        }
        m_queue.push_back(std::move(task));
        if (!m_workerThread.joinable()) {
            m_workerThread = std::thread(&NetworkManager::workerLoop, this);
        }
    }
    m_queueChanged.notify_all();
}

void NetworkManager::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueChanged.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
            if (m_stopping) {
                return;
            }
            task = std::move(m_queue.front());
            m_queue.pop_front();
        }
        task();
    }
}

bool NetworkManager::waitFor(std::chrono::milliseconds duration) {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    return !m_queueChanged.wait_for(lock, duration, [this] { return m_stopping; });
}

void NetworkManager::startScan(const ScanCallback& callback) {
    if (!m_wifiAvailable || !m_running) {
        if (callback) {
            callback({});
        }
        return;
    }

    enqueue([this, callback] { scanWorker(callback); });
}

std::vector<WifiNetwork> NetworkManager::getScanResults() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scanResults;
}

void NetworkManager::connect(const std::string& ssid,
                             const std::string& password,
                             const ConnectCallback& callback) {
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
    notifyStatus();

    enqueue([this, ssid, password, callback] { connectWorker(ssid, password, callback); });
}

void NetworkManager::disconnect() {
    if (!m_wifiAvailable || m_wifiInterface.empty()) return;

    wpaCli({"disconnect"});

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status.state = ConnectionState::Disconnected;
        m_status.ssid.clear();
        m_status.ipAddress.clear();
    }
    notifyStatus();
}

bool NetworkManager::forgetNetwork(const std::string& ssid) {
    if (!m_wifiAvailable) return false;

    // List networks and find the one with matching SSID
    for (const auto& [networkId, networkSsid] : parseNetworkList(wpaCli({"list_networks"}))) {
        if (networkSsid == ssid && isNetworkId(networkId)) {
            wpaCli({"remove_network", networkId});
            wpaCli({"save_config"});
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

    for (const auto& [networkId, ssid] : parseNetworkList(wpaCli({"list_networks"}))) {
        if (!ssid.empty()) {
            networks.push_back(ssid);
        }
    }

    return networks;
}

void NetworkManager::setStatusCallback(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCallback = std::move(callback);
}

void NetworkManager::notifyStatus() {
    StatusCallback callback;
    NetworkStatus status;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_statusCallback;
        status = m_status;
    }
    if (callback) {
        callback(status);
    }
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

std::vector<std::pair<std::string, std::string>> NetworkManager::parseNetworkList(const std::string& output) {
    std::vector<std::pair<std::string, std::string>> networks;
    std::istringstream lines(output);
    std::string line;
    std::getline(lines, line);  // Skip header

    // Format: network_id, ssid, bssid, flags
    while (std::getline(lines, line)) {
        const size_t idEnd = line.find('\t');
        if (idEnd == std::string::npos) {
            continue;
        }
        const size_t ssidEnd = line.find('\t', idEnd + 1);
        networks.emplace_back(line.substr(0, idEnd),
                              decodeWpaText(line.substr(idEnd + 1, ssidEnd == std::string::npos
                                                                       ? ssidEnd
                                                                       : ssidEnd - idEnd - 1)));
    }
    return networks;
}

std::string NetworkManager::decodeWpaText(const std::string& text) {
    // The reverse of wpa_supplicant's printf_encode
    std::string decoded;
    decoded.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 == text.size()) {
            decoded += text[i];
            continue;
        }
        const char escape = text[++i];
        switch (escape) {
            case '\\': decoded += '\\'; break;
            case '"': decoded += '"'; break;
            case 'n': decoded += '\n'; break;
            case 'r': decoded += '\r'; break;
            case 't': decoded += '\t'; break;
            case 'e': decoded += '\x1b'; break;
            case 'x': {
                int value = 0;
                int digits = 0;
                while (digits < 2 && i + 1 < text.size() && hexValue(text[i + 1]) >= 0) {
                    value = value * 16 + hexValue(text[++i]);
                    ++digits;
                }
                if (digits == 0) {
                    decoded += "\\x";
                } else {
                    decoded += static_cast<char>(value);
                }
                break;
            }
            default:
                if (escape >= '0' && escape <= '7') {
                    int value = escape - '0';
                    for (int digits = 1; digits < 3 && i + 1 < text.size() && text[i + 1] >= '0' &&
                                         text[i + 1] <= '7';
                         ++digits) {
                        value = value * 8 + (text[++i] - '0');
                    }
                    decoded += static_cast<char>(value & 0xFF);
                } else {
                    decoded += '\\';
                    decoded += escape;
                }
        }
    }
    return decoded;
}

std::string NetworkManager::ssidToHex(const std::string& ssid) {
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(ssid.size() * 2);
    for (unsigned char c : ssid) {
        hex += digits[c >> 4];
        hex += digits[c & 0xF];
    }
    return hex;
}

std::optional<std::string> NetworkManager::pskArgument(const std::string& password) {
    if (password.size() == 64 &&
        std::all_of(password.begin(), password.end(), [](char c) { return hexValue(c) >= 0; })) {
        return password;  // A raw key, which wpa_supplicant takes unquoted
    }
    if (password.size() < 8 || password.size() > 63) {
        return std::nullopt;
    }
    for (unsigned char c : password) {
        if (c < 0x20 || c == 0x7F) {
            return std::nullopt;
        }
    }
    // wpa_supplicant takes everything up to the last quote, so quotes inside need no escaping
    return "\"" + password + "\"";
}

std::string NetworkManager::wpaCli(const std::vector<std::string>& arguments) const {
    std::vector<std::string> argv{m_options.wpaCli, "-i", m_wifiInterface};
    argv.insert(argv.end(), arguments.begin(), arguments.end());

    const auto result = core::runProcess(argv);
    if (result.exitCode != 0) {
        LOG_DEBUG("NetworkManager: wpa_cli %s returned %d",
                  arguments.empty() ? "" : arguments.front().c_str(), result.exitCode);
    }

    std::string output = result.output;
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    return output;
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

            net.ssid = decodeWpaText(fields[4]);

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

    // Remove duplicates (same SSID), keeping the strongest signal. Duplicates aren't next to each
    // other once sorted by signal, so std::unique wouldn't find them.
    std::vector<WifiNetwork> unique;
    for (auto& net : networks) {
        const bool seen = std::any_of(unique.begin(), unique.end(),
                                      [&](const WifiNetwork& other) { return other.ssid == net.ssid; });
        if (!seen) {
            unique.push_back(std::move(net));
        }
    }

    return unique;
}

void NetworkManager::updateStatus() {
    // wpa_cli runs outside the lock, so getStatus doesn't wait for it
    NetworkStatus status = parseStatusOutput(wpaCli({"status"}));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status = std::move(status);
}

NetworkStatus NetworkManager::parseStatusOutput(const std::string& output) {
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
            status.ssid = decodeWpaText(value);
        } else if (key == "ip_address") {
            status.ipAddress = value;
        }
    }

    return status;
}

void NetworkManager::scanWorker(const ScanCallback& callback) {
    LOG_DEBUG("NetworkManager: Starting WiFi scan...");

    // Request a scan
    wpaCli({"scan"});

    // Wait for scan to complete
    if (!waitFor(m_options.scanWait)) {
        return;
    }

    std::vector<WifiNetwork> networks = parseScanOutput(wpaCli({"scan_results"}));

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
                                   const ConnectCallback& callback) {
    ConnectionResult result;

    auto fail = [&](const std::string& error) {
        result.success = false;
        result.error = error;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_status.state = ConnectionState::Failed;
            m_status.error = error;
        }
        LOG_WARN("NetworkManager: Failed to connect: %s", error.c_str());
        notifyStatus();
        if (callback) callback(result);
    };

    if (ssid.empty() || ssid.size() > kMaxSsidBytes) {
        fail("Network names are 1 to 32 bytes long");
        return;
    }

    std::optional<std::string> psk;
    if (!password.empty()) {
        psk = pskArgument(password);
        if (!psk) {
            fail("Passwords are 8 to 63 characters long, with no control characters");
            return;
        }
    }

    LOG_INFO("NetworkManager: Connecting to a network (%zu bytes)...", ssid.size());

    // Check if network already exists in saved networks
    std::string networkId;
    for (const auto& [id, savedSsid] : parseNetworkList(wpaCli({"list_networks"}))) {
        if (savedSsid == ssid && isNetworkId(id)) {
            networkId = id;
            break;
        }
    }

    auto setNetwork = [&](const std::string& name, const std::string& value) {
        return wpaCli({"set_network", networkId, name, value}) == "OK";
    };

    // If network doesn't exist, create it
    bool added = false;
    if (networkId.empty()) {
        networkId = trim(wpaCli({"add_network"}));
        if (!isNetworkId(networkId)) {
            fail("Failed to create network");
            return;
        }
        added = true;

        bool configured = setNetwork("ssid", ssidToHex(ssid));
        if (configured && !psk) {
            configured = setNetwork("key_mgmt", "NONE");
        }
        if (!configured) {
            wpaCli({"remove_network", networkId});
            fail("Failed to configure network");
            return;
        }
    }

    // Also for a network already listed, as this may be correcting a mistyped password
    if (psk) {
        if (!setNetwork("key_mgmt", "WPA-PSK") ||  // It may have been open
            !setNetwork("psk", *psk)) {
            if (added) {
                wpaCli({"remove_network", networkId});
            }
            fail("Failed to set the password");
            return;
        }
    }

    // Enable the network
    wpaCli({"enable_network", networkId});

    // Select the network (this triggers connection)
    if (wpaCli({"select_network", networkId}).find("OK") == std::string::npos) {
        fail("Failed to select network");
        return;
    }

    // Wait for connection with timeout
    for (int attempt = 0; attempt < m_options.connectPolls; ++attempt) {
        if (!waitFor(m_options.connectPollInterval)) {
            return;  // Shutting down
        }

        updateStatus();

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state == ConnectionState::Connected) {
            break;
        }
    }

    // Check final status
    updateStatus();

    bool connected;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        connected = m_status.state == ConnectionState::Connected;
        result.ipAddress = m_status.ipAddress;
    }

    if (!connected) {
        // Determine error reason
        const std::string statusOutput = wpaCli({"status"});
        if (statusOutput.find("INACTIVE") != std::string::npos ||
            statusOutput.find("DISCONNECTED") != std::string::npos) {
            fail("Incorrect password or network unavailable");
        } else {
            fail("Connection timed out");
        }
        return;
    }

    result.success = true;

    // Save config so it persists across reboots
    if (wpaCli({"save_config"}).find("OK") == std::string::npos) {
        LOG_WARN("NetworkManager: Couldn't save the network, so it won't be there after a reboot");
    }

    LOG_INFO("NetworkManager: Connected, IP: %s", result.ipAddress.c_str());

    notifyStatus();

    if (callback) {
        callback(result);
    }
}

} // namespace network
} // namespace smarthub
