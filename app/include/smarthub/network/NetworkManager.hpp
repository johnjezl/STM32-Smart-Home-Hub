/**
 * Network Manager
 *
 * Manages system WiFi connections using wpa_supplicant (wpa_cli).
 * Provides scanning, connection, and status APIs for the UI.
 */
#pragma once

#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <optional>

namespace smarthub {
namespace network {

/**
 * WiFi network information from scan
 */
struct WifiNetwork {
    std::string ssid;
    std::string bssid;
    int signalStrength = 0;    // dBm, as reported by wpa_cli scan_results
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
 * Where NetworkManager finds its tools, and how long it waits. Tests replace these.
 */
struct NetworkManagerOptions {
    std::string wpaCli = "wpa_cli";
    std::string ip = "ip";
    std::string sysClassNet = "/sys/class/net";
    std::chrono::milliseconds scanWait{3000};
    std::chrono::milliseconds connectPollInterval{500};
    int connectPolls = 30;
};

/**
 * NetworkManager for WiFi configuration
 *
 * Uses wpa_cli (wpa_supplicant CLI) on Linux to manage WiFi. It's run with an argument list, never
 * through a shell, as SSIDs come from whoever is broadcasting them.
 *
 * Scans and connects run one at a time on a worker thread, so they never block the caller.
 */
class NetworkManager {
public:
    using ScanCallback = std::function<void(const std::vector<WifiNetwork>&)>;
    using ConnectCallback = std::function<void(const ConnectionResult&)>;
    using StatusCallback = std::function<void(const NetworkStatus&)>;

    NetworkManager();
    explicit NetworkManager(NetworkManagerOptions options);
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
    void startScan(const ScanCallback& callback);

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
                 const ConnectCallback& callback);

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

    /**
     * Network id to SSID, from the output of wpa_cli list_networks. It's split by tabs, as SSIDs
     * can have spaces, and the SSID is decoded with decodeWpaText.
     */
    static std::vector<std::pair<std::string, std::string>> parseNetworkList(const std::string& output);

    /**
     * Undoes the escaping wpa_supplicant applies to SSIDs in its text output (\\, \", \n, \r,
     * \t, \e, \xHH and octal), giving the SSID's bytes.
     */
    static std::string decodeWpaText(const std::string& text);

    /**
     * An SSID as set_network takes it in hex, which carries any bytes without quoting
     */
    static std::string ssidToHex(const std::string& ssid);

    /**
     * A password as set_network's psk takes it: a passphrase of 8 to 63 characters in quotes, or
     * a raw key of 64 hex digits as is. Nothing if it's neither, or has control characters, which
     * wpa_supplicant would write into its config file as they are.
     */
    static std::optional<std::string> pskArgument(const std::string& password);

private:
    // Runs wpa_cli on the WiFi interface with these arguments, and returns its trimmed output
    std::string wpaCli(const std::vector<std::string>& arguments) const;

    // Parse scan output from wpa_cli scan_results
    std::vector<WifiNetwork> parseScanOutput(const std::string& output);

    // Parse connection status from wpa_cli status
    NetworkStatus parseStatusOutput(const std::string& output);

    // Update current status from wpa_cli
    void updateStatus();

    // Calls the status callback, if any, with the current status
    void notifyStatus();

    // Runs a task on the worker thread, after any already waiting
    void enqueue(std::function<void()> task);
    void workerLoop();
    // Waits, returning false at once if shutting down
    bool waitFor(std::chrono::milliseconds duration);

    void scanWorker(const ScanCallback& callback);
    void connectWorker(const std::string& ssid,
                       const std::string& password,
                       const ConnectCallback& callback);

    NetworkManagerOptions m_options;

    mutable std::mutex m_mutex;
    std::vector<WifiNetwork> m_scanResults;
    NetworkStatus m_status;
    StatusCallback m_statusCallback;

    std::atomic<bool> m_running{false};
    // Connects queued but not started. Their Connecting state stands until they start.
    std::atomic<int> m_connectsQueued{0};

    std::mutex m_queueMutex;
    std::condition_variable m_queueChanged;
    std::deque<std::function<void()>> m_queue;
    bool m_stopping = false;
    std::thread m_workerThread;

    bool m_initialized = false;
    bool m_wifiAvailable = false;
    std::string m_wifiInterface;
};

} // namespace network
} // namespace smarthub
