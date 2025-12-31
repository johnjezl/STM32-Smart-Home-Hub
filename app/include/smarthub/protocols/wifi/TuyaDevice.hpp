/**
 * SmartHub Tuya Local Protocol
 *
 * Implements Tuya local control protocol for WiFi devices.
 * Supports protocol versions 3.1, 3.3, 3.4, and 3.5.
 *
 * Communication is via TCP on port 6668 with AES-encrypted payloads.
 *
 * Message format:
 * +--------+--------+--------+--------+--------+--------+
 * | Prefix | SeqNo  | Cmd    | Length | Data   | Suffix |
 * | 4 bytes| 4 bytes| 4 bytes| 4 bytes| N bytes| 8 bytes|
 * +--------+--------+--------+--------+--------+--------+
 *
 * Encryption:
 * - v3.1/3.3: AES-128-ECB
 * - v3.4/3.5: AES-128-GCM with session keys
 */
#pragma once

#include <smarthub/devices/Device.hpp>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

namespace smarthub::wifi {

/**
 * Tuya protocol command types
 */
enum class TuyaCommand : uint32_t {
    UDP_DISCOVERY = 0x00,
    DP_QUERY = 0x0A,       // Query data points
    CONTROL = 0x07,        // Control device
    STATUS = 0x08,         // Device status
    HEART_BEAT = 0x09,     // Heartbeat ping
    DP_QUERY_NEW = 0x10,   // New query format
    CONTROL_NEW = 0x0D,    // New control format
    SESS_KEY_NEG_START = 0x03,  // Session key negotiation
    SESS_KEY_NEG_RESP = 0x04,
    SESS_KEY_NEG_FINISH = 0x05,
};

/**
 * Tuya device configuration
 */
struct TuyaDeviceConfig {
    std::string deviceId;     // 20-char device ID
    std::string localKey;     // 16-byte AES key (hex or raw)
    std::string ipAddress;
    uint16_t port = 6668;
    std::string version = "3.3";  // Protocol version
    std::string category;      // Device category (switch, light, etc.)
    std::string productId;     // Product ID
    std::string name;          // Device name
};

/**
 * Tuya data point value
 */
struct TuyaDataPoint {
    uint8_t id;
    enum class Type { Raw, Bool, Int, String, Enum } type;
    nlohmann::json value;
};

/**
 * Tuya AES crypto handler
 */
class TuyaCrypto {
public:
    TuyaCrypto() = default;
    explicit TuyaCrypto(const std::string& localKey, const std::string& version);

    void setLocalKey(const std::string& localKey);
    void setVersion(const std::string& version) { m_version = version; }
    void setSessionKey(const std::vector<uint8_t>& key);

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) const;

    std::vector<uint8_t> encrypt(const std::string& data) const;
    std::string decryptToString(const std::vector<uint8_t>& data) const;

    bool needsSessionNegotiation() const;
    std::vector<uint8_t> getLocalNonce() const;
    bool completeSessionNegotiation(const std::vector<uint8_t>& remoteNonce);

private:
    std::vector<uint8_t> aesEcbEncrypt(const std::vector<uint8_t>& data,
                                        const std::vector<uint8_t>& key) const;
    std::vector<uint8_t> aesEcbDecrypt(const std::vector<uint8_t>& data,
                                        const std::vector<uint8_t>& key) const;
    static std::vector<uint8_t> pkcs7Pad(const std::vector<uint8_t>& data, size_t blockSize);
    static std::vector<uint8_t> pkcs7Unpad(const std::vector<uint8_t>& data);

    std::vector<uint8_t> m_localKey;
    std::vector<uint8_t> m_sessionKey;
    std::vector<uint8_t> m_localNonce;
    std::string m_version = "3.3";
    bool m_sessionEstablished = false;
};

/**
 * Tuya protocol message
 */
class TuyaMessage {
public:
    static constexpr uint32_t PREFIX = 0x000055AA;
    static constexpr uint32_t SUFFIX = 0x0000AA55;

    TuyaMessage() = default;
    TuyaMessage(TuyaCommand cmd, uint32_t seqNo = 0);

    void setPayload(const nlohmann::json& payload);
    void setPayload(const std::string& payload);
    void setRawPayload(const std::vector<uint8_t>& data);

    TuyaCommand command() const { return m_command; }
    uint32_t sequenceNumber() const { return m_seqNo; }
    const std::vector<uint8_t>& rawPayload() const { return m_payload; }
    nlohmann::json jsonPayload() const;
    std::string stringPayload() const;

    /**
     * Encode message for transmission
     */
    std::vector<uint8_t> encode(TuyaCrypto& crypto, const std::string& version) const;

    /**
     * Decode received message
     */
    static std::optional<TuyaMessage> decode(const std::vector<uint8_t>& data,
                                              TuyaCrypto& crypto,
                                              const std::string& version);

    /**
     * Find message boundaries in received data
     */
    static std::pair<size_t, size_t> findMessage(const std::vector<uint8_t>& data);

private:
    static uint32_t calculateCrc(const std::vector<uint8_t>& data);

    TuyaCommand m_command = TuyaCommand::STATUS;
    uint32_t m_seqNo = 0;
    std::vector<uint8_t> m_payload;
};

// Callback types
using TuyaStateCallback = std::function<void(const std::string& deviceId,
                                              const std::map<uint8_t, TuyaDataPoint>& dps)>;
using TuyaConnectionCallback = std::function<void(const std::string& deviceId, bool connected)>;

/**
 * Tuya local device controller
 */
class TuyaDevice : public Device {
public:
    TuyaDevice(const std::string& id, const std::string& name,
               const TuyaDeviceConfig& config);
    ~TuyaDevice() override;

    // Non-copyable
    TuyaDevice(const TuyaDevice&) = delete;
    TuyaDevice& operator=(const TuyaDevice&) = delete;

    // Device interface
    bool setState(const std::string& property, const nlohmann::json& value) override;

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const { return m_connected; }

    // Data point operations
    bool setDataPoint(uint8_t dpId, const nlohmann::json& value);
    bool setDataPoints(const std::map<uint8_t, nlohmann::json>& dps);
    bool queryStatus();

    // Callbacks
    void setStateCallback(TuyaStateCallback cb) { m_stateCallback = std::move(cb); }
    void setConnectionCallback(TuyaConnectionCallback cb) { m_connectionCallback = std::move(cb); }

    // Configuration
    const TuyaDeviceConfig& config() const { return m_config; }

private:
    void connectionThread();
    void handleReceive();
    bool sendMessage(const TuyaMessage& msg);
    void processMessage(const TuyaMessage& msg);
    bool performSessionNegotiation();

    TuyaDeviceConfig m_config;
    TuyaCrypto m_crypto;

    int m_socket = -1;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::mutex m_sendMutex;
    uint32_t m_seqNo = 0;

    std::map<uint8_t, TuyaDataPoint> m_dataPoints;
    std::mutex m_dpMutex;

    TuyaStateCallback m_stateCallback;
    TuyaConnectionCallback m_connectionCallback;
};

/**
 * Tuya device UDP discovery
 */
class TuyaDiscovery {
public:
    using DiscoveryCallback = std::function<void(const TuyaDeviceConfig&)>;

    TuyaDiscovery() = default;
    ~TuyaDiscovery();

    /**
     * Start listening for Tuya device broadcasts
     */
    bool start();
    void stop();

    void setCallback(DiscoveryCallback cb) { m_callback = std::move(cb); }

private:
    void listenThread();

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    int m_socket = -1;
    DiscoveryCallback m_callback;
};

} // namespace smarthub::wifi
