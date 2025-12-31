/**
 * SmartHub Shelly Device Support
 *
 * Implements HTTP-based control for Shelly devices.
 * Supports both Gen1 (REST API) and Gen2 (JSON-RPC) devices.
 *
 * Gen1 API:
 * - GET /status - Device status
 * - GET /settings - Device settings
 * - GET /relay/0?turn=on|off|toggle - Control relay
 * - GET /light/0?brightness=X - Control dimmer
 *
 * Gen2 API (JSON-RPC):
 * - POST /rpc with method: "Shelly.GetStatus"
 * - POST /rpc with method: "Switch.Set" / "Light.Set"
 */
#pragma once

#include <smarthub/devices/Device.hpp>
#include <smarthub/protocols/http/HttpClient.hpp>
#include <string>
#include <memory>
#include <vector>

namespace smarthub::wifi {

/**
 * Information about a discovered Shelly device
 */
struct ShellyDeviceInfo {
    std::string id;           // Device ID (e.g., "shellyplug-s-ABCDEF")
    std::string type;         // Device type (e.g., "SHPLG-S", "SHSW-1")
    std::string model;        // Human-readable model
    std::string ipAddress;
    std::string macAddress;
    std::string firmware;
    int generation = 1;       // 1 = Gen1, 2 = Gen2
    int numOutputs = 1;
    bool hasPowerMetering = false;
    bool hasTemperatureSensor = false;
};

/**
 * Shelly output state (relay, light, switch)
 */
struct ShellyOutputState {
    int channel = 0;
    bool isOn = false;
    int brightness = -1;        // 0-100, -1 if not applicable
    int power = 0;              // Watts, if power metering available
    double energy = 0;          // kWh total, if metering available
};

/**
 * Base class for Shelly devices
 */
class ShellyDevice : public Device {
public:
    ShellyDevice(const std::string& id, const std::string& name,
                 const ShellyDeviceInfo& info, http::IHttpClient& http);
    virtual ~ShellyDevice() = default;

    // Device interface
    bool setState(const std::string& property, const nlohmann::json& value) override;

    // Shelly-specific
    const ShellyDeviceInfo& info() const { return m_info; }
    std::string ipAddress() const { return m_info.ipAddress; }
    int generation() const { return m_info.generation; }

    /**
     * Poll device for current status
     * @return true if poll succeeded
     */
    virtual bool pollStatus();

    /**
     * Get output state for a channel
     */
    std::optional<ShellyOutputState> getOutputState(int channel = 0) const;

protected:
    virtual std::string buildUrl(const std::string& path) const;
    void updateOutputState(int channel, const ShellyOutputState& state);

    ShellyDeviceInfo m_info;
    http::IHttpClient& m_http;
    std::vector<ShellyOutputState> m_outputs;
};

/**
 * Shelly Gen1 device (REST API)
 */
class ShellyGen1Device : public ShellyDevice {
public:
    using ShellyDevice::ShellyDevice;

    bool pollStatus() override;

    // Control methods
    bool turnOn(int channel = 0);
    bool turnOff(int channel = 0);
    bool toggle(int channel = 0);
    bool setBrightness(int channel, int level);  // 0-100

protected:
    void parseStatus(const nlohmann::json& status);
};

/**
 * Shelly Gen2 device (JSON-RPC API)
 */
class ShellyGen2Device : public ShellyDevice {
public:
    using ShellyDevice::ShellyDevice;

    bool pollStatus() override;

    // Control methods
    bool turnOn(int channel = 0);
    bool turnOff(int channel = 0);
    bool toggle(int channel = 0);
    bool setBrightness(int channel, int level);

protected:
    std::optional<nlohmann::json> rpcCall(const std::string& method,
                                           const nlohmann::json& params = {});
    void parseGetStatus(const nlohmann::json& result);
};

/**
 * Shelly device discovery via mDNS and HTTP probing
 */
class ShellyDiscovery {
public:
    using DiscoveryCallback = std::function<void(const ShellyDeviceInfo&)>;

    explicit ShellyDiscovery(http::IHttpClient& http);
    ~ShellyDiscovery() = default;

    /**
     * Probe a specific IP address for a Shelly device
     * @param ipAddress IP address to probe
     * @return Device info if found, nullopt otherwise
     */
    std::optional<ShellyDeviceInfo> probeDevice(const std::string& ipAddress);

    /**
     * Set callback for discovered devices
     */
    void setCallback(DiscoveryCallback cb) { m_callback = std::move(cb); }

private:
    std::optional<ShellyDeviceInfo> probeGen1(const std::string& ip);
    std::optional<ShellyDeviceInfo> probeGen2(const std::string& ip);
    std::string getShellyModel(const std::string& type);

    http::IHttpClient& m_http;
    DiscoveryCallback m_callback;
};

/**
 * Create appropriate Shelly device instance based on device info
 */
std::unique_ptr<ShellyDevice> createShellyDevice(
    const ShellyDeviceInfo& info,
    http::IHttpClient& http);

} // namespace smarthub::wifi
