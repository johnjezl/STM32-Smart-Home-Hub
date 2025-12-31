/**
 * SmartHub Shelly Device Implementation
 */

#include <smarthub/protocols/wifi/ShellyDevice.hpp>
#include <smarthub/core/Logger.hpp>
#include <algorithm>

namespace smarthub::wifi {

// Shelly model mapping
static const std::map<std::string, std::string> s_shellyModels = {
    // Gen1 devices
    {"SHSW-1", "Shelly 1"},
    {"SHSW-PM", "Shelly 1PM"},
    {"SHSW-25", "Shelly 2.5"},
    {"SHSW-L", "Shelly 1L"},
    {"SHDM-1", "Shelly Dimmer"},
    {"SHDM-2", "Shelly Dimmer 2"},
    {"SHPLG-S", "Shelly Plug S"},
    {"SHPLG-U1", "Shelly Plug US"},
    {"SHPLG2-1", "Shelly Plug 2"},
    {"SHRGBW2", "Shelly RGBW2"},
    {"SHBLB-1", "Shelly Bulb"},
    {"SHVIN-1", "Shelly Vintage"},
    {"SHBDUO-1", "Shelly Duo"},
    {"SHHT-1", "Shelly H&T"},
    {"SHEM", "Shelly EM"},
    {"SHEM-3", "Shelly 3EM"},
    {"SHSW-21", "Shelly 2"},
    {"SHUNI-1", "Shelly UNI"},
    {"SHMOS-01", "Shelly Motion"},
    {"SHGS-1", "Shelly Gas"},
    {"SHSM-01", "Shelly Smoke"},
    {"SHWT-1", "Shelly Flood"},
    {"SHDW-1", "Shelly Door/Window"},
    {"SHDW-2", "Shelly Door/Window 2"},
    {"SHBTN-1", "Shelly Button1"},
    {"SHBTN-2", "Shelly Button2"},
    {"SHIX3-1", "Shelly i3"},
    // Gen2 devices
    {"SNSW-001X16EU", "Shelly Plus 1"},
    {"SNSW-001P16EU", "Shelly Plus 1PM"},
    {"SNSW-002P16EU", "Shelly Plus 2PM"},
    {"SNPL-00112EU", "Shelly Plus Plug S"},
    {"SNDM-0013US", "Shelly Plus Dimmer"},
    {"SNSN-0024X", "Shelly Plus H&T"},
    {"SPSW-001XE16EU", "Shelly Pro 1"},
    {"SPSW-101PE16EU", "Shelly Pro 1PM"},
    {"SPSW-002PE16EU", "Shelly Pro 2PM"},
    {"SPSW-003PE16EU", "Shelly Pro 3"},
    {"SPEM-003CEBEU", "Shelly Pro 3EM"},
    {"SPSH-002PE16EU", "Shelly Pro Dual Cover PM"},
};

// ============= ShellyDevice =============

ShellyDevice::ShellyDevice(const std::string& id, const std::string& name,
                           const ShellyDeviceInfo& info, http::IHttpClient& http)
    : Device(id, name, DeviceType::Switch)
    , m_info(info)
    , m_http(http) {
    m_outputs.resize(info.numOutputs);

    // Set device type based on model
    if (info.type.find("DM") != std::string::npos ||
        info.type.find("Dimmer") != std::string::npos) {
        m_type = DeviceType::Dimmer;
    } else if (info.type.find("RGB") != std::string::npos ||
               info.type.find("Bulb") != std::string::npos ||
               info.type.find("Duo") != std::string::npos) {
        m_type = DeviceType::ColorLight;
    }

    setProtocol("shelly");
    setAddress(info.ipAddress);
}

bool ShellyDevice::setState(const std::string& property, const nlohmann::json& value) {
    // Subclasses implement specific state changes
    LOG_WARN("setState not implemented for base ShellyDevice");
    return false;
}

bool ShellyDevice::pollStatus() {
    // Subclasses implement
    return false;
}

std::optional<ShellyOutputState> ShellyDevice::getOutputState(int channel) const {
    if (channel >= 0 && channel < static_cast<int>(m_outputs.size())) {
        return m_outputs[channel];
    }
    return std::nullopt;
}

std::string ShellyDevice::buildUrl(const std::string& path) const {
    return "http://" + m_info.ipAddress + path;
}

void ShellyDevice::updateOutputState(int channel, const ShellyOutputState& state) {
    if (channel >= 0 && channel < static_cast<int>(m_outputs.size())) {
        m_outputs[channel] = state;

        // Update device state JSON
        setState("on", state.isOn);
        if (state.brightness >= 0) {
            setState("brightness", state.brightness);
        }
        if (m_info.hasPowerMetering) {
            setState("power", state.power);
            setState("energy", state.energy);
        }
    }
}

// ============= ShellyGen1Device =============

bool ShellyGen1Device::pollStatus() {
    auto response = m_http.getJson(buildUrl("/status"));
    if (!response) {
        LOG_WARN("Failed to poll Shelly Gen1 device %s", m_info.ipAddress.c_str());
        return false;
    }

    parseStatus(*response);
    return true;
}

void ShellyGen1Device::parseStatus(const nlohmann::json& status) {
    try {
        // Parse relays
        if (status.contains("relays") && status["relays"].is_array()) {
            const auto& relays = status["relays"];
            for (size_t i = 0; i < relays.size() && i < m_outputs.size(); i++) {
                ShellyOutputState state;
                state.channel = static_cast<int>(i);
                state.isOn = relays[i].value("ison", false);

                if (status.contains("meters") && status["meters"].is_array() &&
                    i < status["meters"].size()) {
                    state.power = status["meters"][i].value("power", 0);
                    state.energy = status["meters"][i].value("total", 0.0) / 60000.0; // Wh to kWh
                }

                updateOutputState(static_cast<int>(i), state);
            }
        }

        // Parse lights (for dimmers)
        if (status.contains("lights") && status["lights"].is_array()) {
            const auto& lights = status["lights"];
            for (size_t i = 0; i < lights.size() && i < m_outputs.size(); i++) {
                ShellyOutputState state;
                state.channel = static_cast<int>(i);
                state.isOn = lights[i].value("ison", false);
                state.brightness = lights[i].value("brightness", -1);

                if (status.contains("meters") && status["meters"].is_array() &&
                    i < status["meters"].size()) {
                    state.power = status["meters"][i].value("power", 0);
                }

                updateOutputState(static_cast<int>(i), state);
            }
        }

        // Update availability
        setAvailability(DeviceAvailability::Online);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Shelly status: %s", e.what());
    }
}

bool ShellyGen1Device::turnOn(int channel) {
    std::string path = "/relay/" + std::to_string(channel) + "?turn=on";
    auto response = m_http.get(buildUrl(path));
    if (response && response->ok()) {
        LOG_DEBUG("Shelly %s relay %d turned on", m_info.id.c_str(), channel);
        return pollStatus();
    }
    return false;
}

bool ShellyGen1Device::turnOff(int channel) {
    std::string path = "/relay/" + std::to_string(channel) + "?turn=off";
    auto response = m_http.get(buildUrl(path));
    if (response && response->ok()) {
        LOG_DEBUG("Shelly %s relay %d turned off", m_info.id.c_str(), channel);
        return pollStatus();
    }
    return false;
}

bool ShellyGen1Device::toggle(int channel) {
    std::string path = "/relay/" + std::to_string(channel) + "?turn=toggle";
    auto response = m_http.get(buildUrl(path));
    if (response && response->ok()) {
        return pollStatus();
    }
    return false;
}

bool ShellyGen1Device::setBrightness(int channel, int level) {
    level = std::clamp(level, 0, 100);
    std::string path = "/light/" + std::to_string(channel) + "?brightness=" + std::to_string(level);
    auto response = m_http.get(buildUrl(path));
    if (response && response->ok()) {
        return pollStatus();
    }
    return false;
}

// ============= ShellyGen2Device =============

bool ShellyGen2Device::pollStatus() {
    auto result = rpcCall("Shelly.GetStatus");
    if (!result) {
        LOG_WARN("Failed to poll Shelly Gen2 device %s", m_info.ipAddress.c_str());
        return false;
    }

    parseGetStatus(*result);
    return true;
}

std::optional<nlohmann::json> ShellyGen2Device::rpcCall(const std::string& method,
                                                         const nlohmann::json& params) {
    nlohmann::json request = {
        {"id", 1},
        {"method", method}
    };

    if (!params.empty()) {
        request["params"] = params;
    }

    http::HttpRequestOptions opts;
    opts.contentType = "application/json";
    opts.timeoutMs = 5000;

    auto response = m_http.post(buildUrl("/rpc"), request.dump(), opts);
    if (!response || !response->ok()) {
        return std::nullopt;
    }

    try {
        auto json = nlohmann::json::parse(response->body);
        if (json.contains("result")) {
            return json["result"];
        } else if (json.contains("error")) {
            LOG_ERROR("Shelly RPC error: %s", json["error"].dump().c_str());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Shelly RPC response: %s", e.what());
    }

    return std::nullopt;
}

void ShellyGen2Device::parseGetStatus(const nlohmann::json& result) {
    try {
        // Parse switches
        for (int i = 0; i < m_info.numOutputs; i++) {
            std::string key = "switch:" + std::to_string(i);
            if (result.contains(key)) {
                const auto& sw = result[key];
                ShellyOutputState state;
                state.channel = i;
                state.isOn = sw.value("output", false);

                if (sw.contains("apower")) {
                    state.power = sw.value("apower", 0);
                }
                if (sw.contains("aenergy")) {
                    auto& energy = sw["aenergy"];
                    if (energy.contains("total")) {
                        state.energy = energy["total"].get<double>() / 1000.0; // Wh to kWh
                    }
                }

                updateOutputState(i, state);
            }
        }

        // Parse lights (for Plus Dimmer, etc.)
        for (int i = 0; i < m_info.numOutputs; i++) {
            std::string key = "light:" + std::to_string(i);
            if (result.contains(key)) {
                const auto& light = result[key];
                ShellyOutputState state;
                state.channel = i;
                state.isOn = light.value("output", false);
                state.brightness = light.value("brightness", -1);

                updateOutputState(i, state);
            }
        }

        setAvailability(DeviceAvailability::Online);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Shelly Gen2 status: %s", e.what());
    }
}

bool ShellyGen2Device::turnOn(int channel) {
    auto result = rpcCall("Switch.Set", {{"id", channel}, {"on", true}});
    if (result) {
        return pollStatus();
    }
    return false;
}

bool ShellyGen2Device::turnOff(int channel) {
    auto result = rpcCall("Switch.Set", {{"id", channel}, {"on", false}});
    if (result) {
        return pollStatus();
    }
    return false;
}

bool ShellyGen2Device::toggle(int channel) {
    auto result = rpcCall("Switch.Toggle", {{"id", channel}});
    if (result) {
        return pollStatus();
    }
    return false;
}

bool ShellyGen2Device::setBrightness(int channel, int level) {
    level = std::clamp(level, 0, 100);
    auto result = rpcCall("Light.Set", {{"id", channel}, {"brightness", level}});
    if (result) {
        return pollStatus();
    }
    return false;
}

// ============= ShellyDiscovery =============

ShellyDiscovery::ShellyDiscovery(http::IHttpClient& http)
    : m_http(http) {
}

std::optional<ShellyDeviceInfo> ShellyDiscovery::probeDevice(const std::string& ipAddress) {
    // Try Gen2 first (newer, more common for new devices)
    auto gen2 = probeGen2(ipAddress);
    if (gen2) {
        if (m_callback) {
            m_callback(*gen2);
        }
        return gen2;
    }

    // Try Gen1
    auto gen1 = probeGen1(ipAddress);
    if (gen1) {
        if (m_callback) {
            m_callback(*gen1);
        }
        return gen1;
    }

    return std::nullopt;
}

std::optional<ShellyDeviceInfo> ShellyDiscovery::probeGen1(const std::string& ip) {
    auto settings = m_http.getJson("http://" + ip + "/settings");
    if (!settings) {
        return std::nullopt;
    }

    try {
        ShellyDeviceInfo info;
        info.ipAddress = ip;
        info.generation = 1;

        // Parse settings
        if (settings->contains("device")) {
            const auto& dev = (*settings)["device"];
            info.type = dev.value("type", "");
            info.macAddress = dev.value("mac", "");
            info.id = dev.value("hostname", "shelly-" + info.macAddress);
            info.numOutputs = dev.value("num_outputs", 1);
        }

        if (settings->contains("fw")) {
            info.firmware = (*settings)["fw"].get<std::string>();
        }

        if (settings->contains("meters")) {
            info.hasPowerMetering = (*settings)["meters"].is_array() &&
                                     !(*settings)["meters"].empty();
        }

        info.model = getShellyModel(info.type);

        LOG_INFO("Discovered Shelly Gen1 device: %s (%s) at %s",
                 info.model.c_str(), info.type.c_str(), ip.c_str());

        return info;

    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to parse Shelly Gen1 settings: %s", e.what());
    }

    return std::nullopt;
}

std::optional<ShellyDeviceInfo> ShellyDiscovery::probeGen2(const std::string& ip) {
    // Gen2 uses JSON-RPC
    nlohmann::json request = {
        {"id", 1},
        {"method", "Shelly.GetDeviceInfo"}
    };

    http::HttpRequestOptions opts;
    opts.contentType = "application/json";
    opts.timeoutMs = 3000;

    auto response = m_http.post("http://" + ip + "/rpc", request.dump(), opts);
    if (!response || !response->ok()) {
        return std::nullopt;
    }

    try {
        auto json = nlohmann::json::parse(response->body);
        if (!json.contains("result")) {
            return std::nullopt;
        }

        const auto& result = json["result"];

        ShellyDeviceInfo info;
        info.ipAddress = ip;
        info.generation = 2;
        info.id = result.value("id", "");
        info.macAddress = result.value("mac", "");
        info.type = result.value("model", "");
        info.firmware = result.value("fw_id", "");

        // Get number of outputs from profile
        if (result.contains("profile")) {
            std::string profile = result["profile"];
            if (profile == "cover") {
                info.numOutputs = 1;
            } else if (profile == "2ch" || profile == "2-outputs") {
                info.numOutputs = 2;
            } else if (profile == "4ch" || profile == "4-outputs") {
                info.numOutputs = 4;
            }
        }

        // Get switch count from GetStatus
        nlohmann::json statusReq = {
            {"id", 2},
            {"method", "Shelly.GetStatus"}
        };
        auto statusResp = m_http.post("http://" + ip + "/rpc", statusReq.dump(), opts);
        if (statusResp && statusResp->ok()) {
            auto statusJson = nlohmann::json::parse(statusResp->body);
            if (statusJson.contains("result")) {
                const auto& status = statusJson["result"];
                int count = 0;
                for (int i = 0; i < 8; i++) {
                    std::string key = "switch:" + std::to_string(i);
                    if (status.contains(key)) {
                        count++;
                        if (status[key].contains("apower")) {
                            info.hasPowerMetering = true;
                        }
                    }
                }
                if (count > 0) {
                    info.numOutputs = count;
                }
            }
        }

        info.model = getShellyModel(info.type);
        if (info.model.empty()) {
            info.model = info.type;
        }

        LOG_INFO("Discovered Shelly Gen2 device: %s (%s) at %s",
                 info.model.c_str(), info.type.c_str(), ip.c_str());

        return info;

    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to parse Shelly Gen2 response: %s", e.what());
    }

    return std::nullopt;
}

std::string ShellyDiscovery::getShellyModel(const std::string& type) {
    auto it = s_shellyModels.find(type);
    if (it != s_shellyModels.end()) {
        return it->second;
    }
    return type;
}

// ============= Factory =============

std::unique_ptr<ShellyDevice> createShellyDevice(
    const ShellyDeviceInfo& info,
    http::IHttpClient& http) {

    std::string deviceId = "shelly_" + info.id;
    std::string name = info.model + " (" + info.ipAddress + ")";

    if (info.generation == 2) {
        return std::make_unique<ShellyGen2Device>(deviceId, name, info, http);
    } else {
        return std::make_unique<ShellyGen1Device>(deviceId, name, info, http);
    }
}

} // namespace smarthub::wifi
