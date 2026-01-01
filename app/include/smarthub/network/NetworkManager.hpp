/**
 * Network Manager
 *
 * Manages system WiFi connections using NetworkManager (nmcli).
 * Provides scanning, connection, and status APIs for the UI.
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

namespace smarthub {
namespace network {

/**
 * WiFi network information from scan
 */
struct WifiNetwork {
    std::string ssid;
    std::string bssid;
    int signalStrength = 0;    // 0-100 percentage
    bool secured = false;       // Has encryption
    bool connected = false;     // Currently connected
    std::string security;       // "WPA2", "WPA3", "WEP", "Open"
    int frequency = 0;          // MHz (2400 = 2.4GHz, 5000 = 5GHz)
};

/**
 * Connection state
 */
enum class ConnectionState {
    Disconnected,
    Scanning,
    Connecting,
    Connected,
    Failed
};

/**
 * Connection result
 */
struct ConnectionResult {
    bool success = false;
    std::string error;
    std::string ipAddress;
};

/**
 * Network status
 */
struct NetworkStatus {
    ConnectionState state = ConnectionState::Disconnected;
    std::string ssid;
    std::string ipAddress;
    int signalStrength = 0;
    std::string error;
};

/**
 * NetworkManager for WiFi configuration
 *
 * Uses nmcli (NetworkManager CLI) on Linux to manage WiFi.
 */
class NetworkManager {
public:
    using ScanCallback = std::function<void(const std::vector<WifiNetwork>&)>;
    using ConnectCallback = std::function<void(const ConnectionResult&)>;
    using StatusCallback = std::function<void(const NetworkStatus&)>;

    NetworkManager();
    ~NetworkManager();

    /**
     * Initialize network manager
     * @return true if NetworkManager is available
     */
    bool initialize();

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    /**
     * Check if WiFi hardware is available
     */
    bool isWifiAvailable() const;

    /**
     * Enable/disable WiFi radio
     */
    bool setWifiEnabled(bool enabled);
    bool isWifiEnabled() const;

    /**
     * Start async WiFi scan
     * Results delivered via callback
     */
    void startScan(ScanCallback callback);

    /**
     * Get cached scan results (last scan)
     */
    std::vector<WifiNetwork> getScanResults() const;

    /**
     * Connect to a WiFi network
     * @param ssid Network SSID
     * @param password Password (empty for open networks)
     * @param callback Result callback
     */
    void connect(const std::string& ssid,
                 const std::string& password,
                 ConnectCallback callback);

    /**
     * Disconnect from current network
     */
    void disconnect();

    /**
     * Forget a saved network
     */
    bool forgetNetwork(const std::string& ssid);

    /**
     * Get current network status
     */
    NetworkStatus getStatus() const;

    /**
     * Get list of saved networks
     */
    std::vector<std::string> getSavedNetworks() const;

    /**
     * Set status change callback
     */
    void setStatusCallback(StatusCallback callback);

    /**
     * Get signal strength icon index (0-4)
     */
    static int signalToIconIndex(int signalStrength);

    /**
     * Convert signal dBm to percentage
     */
    static int dbmToPercent(int dbm);

private:
    // Execute nmcli command and return output
    std::string executeCommand(const std::string& command) const;

    // Parse scan output
    std::vector<WifiNetwork> parseScanOutput(const std::string& output);

    // Parse connection status
    NetworkStatus parseStatusOutput(const std::string& output);

    // Worker thread for async operations
    void scanWorker(ScanCallback callback);
    void connectWorker(const std::string& ssid,
                       const std::string& password,
                       ConnectCallback callback);

    mutable std::mutex m_mutex;
    std::vector<WifiNetwork> m_scanResults;
    NetworkStatus m_status;
    StatusCallback m_statusCallback;

    std::atomic<bool> m_running{false};
    std::unique_ptr<std::thread> m_workerThread;

    bool m_initialized = false;
    bool m_wifiAvailable = false;
    std::string m_wifiInterface;
};

} // namespace network
} // namespace smarthub
