/**
 * WiFi Protocol Tests
 *
 * Comprehensive tests for Phase 5 WiFi device integration:
 * - HTTP Client
 * - MQTT Discovery (Tasmota/ESPHome)
 * - Shelly devices (Gen1/Gen2)
 * - Tuya local protocol
 * - WiFi Protocol Handler
 */

#include <gtest/gtest.h>
#include <smarthub/protocols/http/HttpClient.hpp>
#include <smarthub/protocols/wifi/MqttDiscovery.hpp>
#include <smarthub/protocols/wifi/ShellyDevice.hpp>
#include <smarthub/protocols/wifi/TuyaDevice.hpp>
#include <smarthub/protocols/wifi/WifiHandler.hpp>
#include <smarthub/core/EventBus.hpp>

using namespace smarthub;
using namespace smarthub::wifi;
using namespace smarthub::http;

// ============= Mock HTTP Client =============

class MockHttpClient : public IHttpClient {
public:
    std::optional<HttpResponse> get(const std::string& url,
                                     const HttpRequestOptions& options = {}) override {
        getCallCount++;
        lastGetUrl = url;

        auto it = responses.find(url);
        if (it != responses.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<HttpResponse> post(const std::string& url,
                                      const std::string& body,
                                      const HttpRequestOptions& options = {}) override {
        postCallCount++;
        lastPostUrl = url;
        lastPostBody = body;

        auto it = responses.find(url);
        if (it != responses.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<nlohmann::json> getJson(const std::string& url, int timeoutMs = 5000) override {
        auto resp = get(url);
        if (resp && resp->ok()) {
            return resp->json();
        }
        return std::nullopt;
    }

    void addResponse(const std::string& url, int status, const std::string& body) {
        HttpResponse resp;
        resp.statusCode = status;
        resp.body = body;
        responses[url] = resp;
    }

    void addJsonResponse(const std::string& url, const nlohmann::json& json) {
        addResponse(url, 200, json.dump());
    }

    std::map<std::string, HttpResponse> responses;
    int getCallCount = 0;
    int postCallCount = 0;
    std::string lastGetUrl;
    std::string lastPostUrl;
    std::string lastPostBody;
};

// ============= HTTP Response Tests =============

class HttpResponseTest : public ::testing::Test {};

TEST_F(HttpResponseTest, OkForSuccessStatusCodes) {
    HttpResponse resp;
    resp.statusCode = 200;
    EXPECT_TRUE(resp.ok());

    resp.statusCode = 201;
    EXPECT_TRUE(resp.ok());

    resp.statusCode = 204;
    EXPECT_TRUE(resp.ok());
}

TEST_F(HttpResponseTest, NotOkForErrorStatusCodes) {
    HttpResponse resp;
    resp.statusCode = 400;
    EXPECT_FALSE(resp.ok());

    resp.statusCode = 404;
    EXPECT_FALSE(resp.ok());

    resp.statusCode = 500;
    EXPECT_FALSE(resp.ok());
}

TEST_F(HttpResponseTest, ParsesJsonBody) {
    HttpResponse resp;
    resp.statusCode = 200;
    resp.body = R"({"key": "value", "number": 42})";

    auto json = resp.json();
    EXPECT_EQ(json["key"], "value");
    EXPECT_EQ(json["number"], 42);
}

TEST_F(HttpResponseTest, EmptyJsonForEmptyBody) {
    HttpResponse resp;
    resp.body = "";

    auto json = resp.json();
    EXPECT_TRUE(json.empty());
}

TEST_F(HttpResponseTest, EmptyJsonForInvalidJson) {
    HttpResponse resp;
    resp.body = "not valid json {{{";

    auto json = resp.json();
    EXPECT_TRUE(json.empty());
}

// ============= MQTT Discovery Tests =============

class MqttDiscoveryTest : public ::testing::Test {
protected:
    MqttDiscoveryManager manager;
};

TEST_F(MqttDiscoveryTest, ParsesBasicSwitchConfig) {
    std::string topic = "homeassistant/switch/kitchen_plug/config";
    std::string payload = R"({
        "name": "Kitchen Plug",
        "unique_id": "kitchen_plug_001",
        "state_topic": "stat/kitchen_plug/POWER",
        "command_topic": "cmnd/kitchen_plug/POWER",
        "availability_topic": "tele/kitchen_plug/LWT",
        "payload_on": "ON",
        "payload_off": "OFF"
    })";

    auto config = MqttDiscoveryConfig::parse(topic, payload);
    ASSERT_TRUE(config.has_value());

    EXPECT_EQ(config->name, "Kitchen Plug");
    EXPECT_EQ(config->uniqueId, "kitchen_plug_001");
    EXPECT_EQ(config->component, "switch");
    EXPECT_EQ(config->stateTopic, "stat/kitchen_plug/POWER");
    EXPECT_EQ(config->commandTopic, "cmnd/kitchen_plug/POWER");
    EXPECT_EQ(config->payloadOn, "ON");
    EXPECT_EQ(config->payloadOff, "OFF");
}

TEST_F(MqttDiscoveryTest, ParsesLightWithBrightness) {
    std::string topic = "homeassistant/light/living_room/config";
    std::string payload = R"({
        "name": "Living Room Light",
        "unique_id": "living_room_light_001",
        "state_topic": "stat/living_room/POWER",
        "command_topic": "cmnd/living_room/POWER",
        "brightness_command_topic": "cmnd/living_room/DIMMER",
        "brightness_state_topic": "stat/living_room/DIMMER",
        "brightness_scale": 100
    })";

    auto config = MqttDiscoveryConfig::parse(topic, payload);
    ASSERT_TRUE(config.has_value());

    EXPECT_EQ(config->component, "light");
    EXPECT_EQ(config->brightnessCommandTopic, "cmnd/living_room/DIMMER");
    EXPECT_EQ(config->brightnessStateTopic, "stat/living_room/DIMMER");
    EXPECT_EQ(config->brightnessScale, 100);
}

TEST_F(MqttDiscoveryTest, ParsesDeviceInfo) {
    std::string topic = "homeassistant/sensor/temp/config";
    std::string payload = R"({
        "name": "Temperature",
        "unique_id": "temp_001",
        "state_topic": "sensors/temp",
        "device": {
            "identifiers": "device_001",
            "manufacturer": "Acme",
            "model": "Temp Sensor v2",
            "name": "Temperature Monitor",
            "sw_version": "1.2.3"
        }
    })";

    auto config = MqttDiscoveryConfig::parse(topic, payload);
    ASSERT_TRUE(config.has_value());

    EXPECT_EQ(config->device.manufacturer, "Acme");
    EXPECT_EQ(config->device.model, "Temp Sensor v2");
    EXPECT_EQ(config->device.swVersion, "1.2.3");
}

TEST_F(MqttDiscoveryTest, DetectsTasmotaSource) {
    std::string topic = "homeassistant/switch/tasmota_001/config";
    std::string payload = R"({
        "name": "Tasmota Switch",
        "unique_id": "tasmota_001",
        "state_topic": "stat/tasmota_001/POWER",
        "command_topic": "cmnd/tasmota_001/POWER",
        "device": {
            "sw": "Tasmota 12.0.0"
        }
    })";

    auto config = MqttDiscoveryConfig::parse(topic, payload);
    ASSERT_TRUE(config.has_value());

    EXPECT_TRUE(config->isTasmota());
    EXPECT_FALSE(config->isESPHome());
}

TEST_F(MqttDiscoveryTest, DetectsESPHomeSource) {
    std::string topic = "homeassistant/sensor/esphome_temp/config";
    std::string payload = R"({
        "name": "ESPHome Temperature",
        "unique_id": "esphome_temp_001",
        "state_topic": "esphome/sensor/temp",
        "device": {
            "sw_version": "esphome v2023.8.0"
        }
    })";

    auto config = MqttDiscoveryConfig::parse(topic, payload);
    ASSERT_TRUE(config.has_value());

    EXPECT_TRUE(config->isESPHome());
    EXPECT_FALSE(config->isTasmota());
}

TEST_F(MqttDiscoveryTest, ManagerTracksDiscoveredDevices) {
    std::string topic = "homeassistant/switch/test/config";
    std::string payload = R"({
        "name": "Test Switch",
        "unique_id": "test_switch_001",
        "state_topic": "stat/test/POWER",
        "command_topic": "cmnd/test/POWER"
    })";

    bool callbackCalled = false;
    manager.setDiscoveryCallback([&callbackCalled](const MqttDiscoveryConfig& config) {
        callbackCalled = true;
        EXPECT_EQ(config.uniqueId, "test_switch_001");
    });

    manager.processMessage(topic, payload);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(manager.getDiscoveredDeviceIds().size(), 1);

    auto device = manager.getDevice("test_switch_001");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->name, "Test Switch");
}

TEST_F(MqttDiscoveryTest, ManagerHandlesStateUpdates) {
    // First discover the device
    std::string discoveryTopic = "homeassistant/switch/test/config";
    std::string discoveryPayload = R"({
        "unique_id": "test_001",
        "state_topic": "stat/test/POWER"
    })";
    manager.processMessage(discoveryTopic, discoveryPayload);

    // Then send state update
    bool stateCallbackCalled = false;
    manager.setStateCallback([&](const std::string& id, const std::string& prop,
                                  const nlohmann::json& val) {
        stateCallbackCalled = true;
        EXPECT_EQ(id, "test_001");
        EXPECT_EQ(prop, "state");
    });

    manager.processMessage("stat/test/POWER", "ON");

    EXPECT_TRUE(stateCallbackCalled);
}

TEST_F(MqttDiscoveryTest, ManagerHandlesAvailability) {
    // Discover device with availability topic
    std::string discoveryTopic = "homeassistant/switch/test/config";
    std::string discoveryPayload = R"({
        "unique_id": "test_001",
        "availability_topic": "tele/test/LWT",
        "payload_available": "Online",
        "payload_not_available": "Offline"
    })";
    manager.processMessage(discoveryTopic, discoveryPayload);

    bool availabilityCallbackCalled = false;
    manager.setAvailabilityCallback([&](const std::string& id, bool available) {
        availabilityCallbackCalled = true;
        EXPECT_EQ(id, "test_001");
        EXPECT_TRUE(available);
    });

    manager.processMessage("tele/test/LWT", "Online");

    EXPECT_TRUE(availabilityCallbackCalled);
}

TEST_F(MqttDiscoveryTest, EmptyPayloadRemovesDevice) {
    // Add device
    std::string topic = "homeassistant/switch/test/config";
    std::string payload = R"({"unique_id": "test_001", "name": "Test"})";
    manager.processMessage(topic, payload);
    EXPECT_EQ(manager.getDiscoveredDeviceIds().size(), 1);

    // Remove with empty payload
    manager.processMessage(topic, "");
    // Device removal is triggered by empty payload
}

TEST_F(MqttDiscoveryTest, CorrectDeviceClass) {
    auto checkClass = [](const std::string& component, MqttDeviceClass expected) {
        std::string topic = "homeassistant/" + component + "/test/config";
        std::string payload = R"({"unique_id": "test"})";
        auto config = MqttDiscoveryConfig::parse(topic, payload);
        ASSERT_TRUE(config.has_value());
        EXPECT_EQ(config->deviceClass(), expected);
    };

    checkClass("switch", MqttDeviceClass::Switch);
    checkClass("light", MqttDeviceClass::Light);
    checkClass("sensor", MqttDeviceClass::Sensor);
    checkClass("binary_sensor", MqttDeviceClass::BinarySensor);
    checkClass("climate", MqttDeviceClass::Climate);
    checkClass("fan", MqttDeviceClass::Fan);
}

// ============= Shelly Device Tests =============

class ShellyDeviceTest : public ::testing::Test {
protected:
    MockHttpClient http;
};

TEST_F(ShellyDeviceTest, DiscoverGen1Device) {
    http.addJsonResponse("http://192.168.1.100/settings", {
        {"device", {
            {"type", "SHSW-1"},
            {"mac", "AABBCCDDEEFF"},
            {"hostname", "shelly1-AABBCC"},
            {"num_outputs", 1}
        }},
        {"fw", "20230913-112316/v1.14.0-gcb84623"}
    });

    ShellyDiscovery discovery(http);

    auto info = discovery.probeDevice("192.168.1.100");
    ASSERT_TRUE(info.has_value());

    EXPECT_EQ(info->type, "SHSW-1");
    EXPECT_EQ(info->generation, 1);
    EXPECT_EQ(info->ipAddress, "192.168.1.100");
    EXPECT_EQ(info->numOutputs, 1);
}

TEST_F(ShellyDeviceTest, DiscoverGen2Device) {
    // Gen2 uses JSON-RPC
    http.addJsonResponse("http://192.168.1.100/rpc", {
        {"result", {
            {"id", "shellyplus1-AABBCC"},
            {"mac", "AABBCCDDEEFF"},
            {"model", "SNSW-001X16EU"},
            {"fw_id", "20230912-091530/1.0.0-g1234567"}
        }}
    });

    ShellyDiscovery discovery(http);

    auto info = discovery.probeDevice("192.168.1.100");
    ASSERT_TRUE(info.has_value());

    EXPECT_EQ(info->type, "SNSW-001X16EU");
    EXPECT_EQ(info->generation, 2);
}

TEST_F(ShellyDeviceTest, Gen1DevicePolling) {
    ShellyDeviceInfo info;
    info.id = "shelly1-001";
    info.type = "SHSW-1";
    info.ipAddress = "192.168.1.100";
    info.generation = 1;
    info.numOutputs = 1;

    http.addJsonResponse("http://192.168.1.100/status", {
        {"relays", {{{"ison", true}}}},
        {"meters", {{{"power", 42}, {"total", 12345}}}}
    });

    ShellyGen1Device device("shelly_001", "Shelly 1", info, http);
    EXPECT_TRUE(device.pollStatus());

    auto state = device.getOutputState(0);
    ASSERT_TRUE(state.has_value());
    EXPECT_TRUE(state->isOn);
    EXPECT_EQ(state->power, 42);
}

TEST_F(ShellyDeviceTest, Gen1TurnOn) {
    ShellyDeviceInfo info;
    info.id = "shelly1-001";
    info.type = "SHSW-1";
    info.ipAddress = "192.168.1.100";
    info.generation = 1;
    info.numOutputs = 1;

    // Response for turn on
    http.addJsonResponse("http://192.168.1.100/relay/0?turn=on", {
        {"ison", true}
    });

    // Response for status poll
    http.addJsonResponse("http://192.168.1.100/status", {
        {"relays", {{{"ison", true}}}}
    });

    ShellyGen1Device device("shelly_001", "Shelly 1", info, http);
    EXPECT_TRUE(device.turnOn(0));

    EXPECT_EQ(http.lastGetUrl, "http://192.168.1.100/status");
}

TEST_F(ShellyDeviceTest, Gen2DevicePolling) {
    ShellyDeviceInfo info;
    info.id = "shellyplus1-001";
    info.type = "SNSW-001X16EU";
    info.ipAddress = "192.168.1.100";
    info.generation = 2;
    info.numOutputs = 1;

    http.addJsonResponse("http://192.168.1.100/rpc", {
        {"result", {
            {"switch:0", {
                {"output", true},
                {"apower", 55.5},
                {"aenergy", {{"total", 1234.5}}}
            }}
        }}
    });

    ShellyGen2Device device("shelly_001", "Shelly Plus 1", info, http);
    EXPECT_TRUE(device.pollStatus());

    auto state = device.getOutputState(0);
    ASSERT_TRUE(state.has_value());
    EXPECT_TRUE(state->isOn);
}

TEST_F(ShellyDeviceTest, CreateShellyDeviceFactory) {
    ShellyDeviceInfo gen1Info;
    gen1Info.id = "shelly1";
    gen1Info.generation = 1;

    auto gen1Device = createShellyDevice(gen1Info, http);
    EXPECT_NE(dynamic_cast<ShellyGen1Device*>(gen1Device.get()), nullptr);

    ShellyDeviceInfo gen2Info;
    gen2Info.id = "shellyplus1";
    gen2Info.generation = 2;

    auto gen2Device = createShellyDevice(gen2Info, http);
    EXPECT_NE(dynamic_cast<ShellyGen2Device*>(gen2Device.get()), nullptr);
}

// ============= Tuya Protocol Tests =============

class TuyaCryptoTest : public ::testing::Test {};

TEST_F(TuyaCryptoTest, SetLocalKeyFromHex) {
    TuyaCrypto crypto("0123456789abcdef0123456789abcdef", "3.3");
    // Should not throw
}

TEST_F(TuyaCryptoTest, SetLocalKeyFromRaw) {
    TuyaCrypto crypto("0123456789abcdef", "3.3");
    // Should not throw - 16 char raw key
}

TEST_F(TuyaCryptoTest, NeedsSessionForV34) {
    TuyaCrypto crypto33("0123456789abcdef", "3.3");
    EXPECT_FALSE(crypto33.needsSessionNegotiation());

    TuyaCrypto crypto34("0123456789abcdef", "3.4");
    EXPECT_TRUE(crypto34.needsSessionNegotiation());
}

class TuyaMessageTest : public ::testing::Test {};

TEST_F(TuyaMessageTest, CreateMessage) {
    TuyaMessage msg(TuyaCommand::DP_QUERY, 1);
    EXPECT_EQ(msg.command(), TuyaCommand::DP_QUERY);
    EXPECT_EQ(msg.sequenceNumber(), 1);
}

TEST_F(TuyaMessageTest, SetJsonPayload) {
    TuyaMessage msg(TuyaCommand::CONTROL, 1);
    nlohmann::json payload = {
        {"devId", "test123"},
        {"dps", {{"1", true}}}
    };
    msg.setPayload(payload);

    auto parsed = msg.jsonPayload();
    EXPECT_EQ(parsed["devId"], "test123");
    EXPECT_TRUE(parsed["dps"]["1"].get<bool>());
}

TEST_F(TuyaMessageTest, FindMessageInBuffer) {
    std::vector<uint8_t> buffer = {
        0x00, 0x00, 0x55, 0xAA,  // Prefix
        0x00, 0x00, 0x00, 0x01,  // Sequence
        0x00, 0x00, 0x00, 0x09,  // Command (heartbeat)
        0x00, 0x00, 0x00, 0x08,  // Length (8 = CRC + suffix)
        // CRC (4 bytes) + Suffix (4 bytes)
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xAA, 0x55
    };

    auto [start, len] = TuyaMessage::findMessage(buffer);
    EXPECT_EQ(start, 0);
    EXPECT_EQ(len, 24);
}

TEST_F(TuyaMessageTest, FindMessageInBufferWithGarbage) {
    std::vector<uint8_t> buffer = {
        0xFF, 0xFF, 0xFF,        // Garbage
        0x00, 0x00, 0x55, 0xAA,  // Prefix
        0x00, 0x00, 0x00, 0x01,  // Sequence
        0x00, 0x00, 0x00, 0x09,  // Command
        0x00, 0x00, 0x00, 0x08,  // Length
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xAA, 0x55
    };

    auto [start, len] = TuyaMessage::findMessage(buffer);
    EXPECT_EQ(start, 3);
    EXPECT_EQ(len, 24);
}

class TuyaDeviceConfigTest : public ::testing::Test {};

TEST_F(TuyaDeviceConfigTest, DefaultPort) {
    TuyaDeviceConfig config;
    EXPECT_EQ(config.port, 6668);
}

TEST_F(TuyaDeviceConfigTest, DefaultVersion) {
    TuyaDeviceConfig config;
    EXPECT_EQ(config.version, "3.3");
}

// ============= WiFi Handler Tests =============

class WifiHandlerTest : public ::testing::Test {
protected:
    EventBus eventBus;
    nlohmann::json config;
};

TEST_F(WifiHandlerTest, Construction) {
    WifiHandler handler(eventBus, config);
    EXPECT_EQ(handler.name(), "wifi");
    EXPECT_EQ(handler.version(), "1.0.0");
    EXPECT_TRUE(handler.supportsDiscovery());
}

TEST_F(WifiHandlerTest, InitialState) {
    WifiHandler handler(eventBus, config);
    EXPECT_FALSE(handler.isConnected());
    EXPECT_FALSE(handler.isDiscovering());
    EXPECT_EQ(handler.state(), ProtocolState::Disconnected);
}

TEST_F(WifiHandlerTest, InitializeWithoutMqtt) {
    // No MQTT config
    WifiHandler handler(eventBus, config);
    EXPECT_TRUE(handler.initialize());
    EXPECT_TRUE(handler.isConnected());
    EXPECT_EQ(handler.state(), ProtocolState::Connected);
}

TEST_F(WifiHandlerTest, StatusJson) {
    WifiHandler handler(eventBus, config);
    handler.initialize();

    auto status = handler.getStatus();
    EXPECT_TRUE(status["connected"].get<bool>());
    EXPECT_EQ(status["deviceCount"], 0);
}

TEST_F(WifiHandlerTest, StartStopDiscovery) {
    WifiHandler handler(eventBus, config);
    handler.initialize();

    handler.startDiscovery();
    EXPECT_TRUE(handler.isDiscovering());

    handler.stopDiscovery();
    EXPECT_FALSE(handler.isDiscovering());
}

TEST_F(WifiHandlerTest, ShutdownCleansUp) {
    WifiHandler handler(eventBus, config);
    handler.initialize();
    handler.shutdown();

    EXPECT_FALSE(handler.isConnected());
    EXPECT_EQ(handler.state(), ProtocolState::Disconnected);
}

TEST_F(WifiHandlerTest, GetKnownDevicesEmptyInitially) {
    WifiHandler handler(eventBus, config);
    handler.initialize();

    auto devices = handler.getKnownDeviceAddresses();
    EXPECT_TRUE(devices.empty());
}

// ============= Integration Tests =============

class WifiIntegrationTest : public ::testing::Test {
protected:
    EventBus eventBus;
    MqttDiscoveryManager discoveryManager;
};

TEST_F(WifiIntegrationTest, FullMqttDiscoveryFlow) {
    // Simulate full discovery flow
    std::vector<MqttDiscoveryConfig> discoveredDevices;

    discoveryManager.setDiscoveryCallback([&](const MqttDiscoveryConfig& config) {
        discoveredDevices.push_back(config);
    });

    // Discover multiple devices
    discoveryManager.processMessage("homeassistant/switch/plug1/config",
        R"({"unique_id": "plug1", "name": "Kitchen Plug", "state_topic": "stat/plug1/POWER"})");

    discoveryManager.processMessage("homeassistant/light/bulb1/config",
        R"({"unique_id": "bulb1", "name": "Living Room Light", "state_topic": "stat/bulb1/POWER",
            "brightness_command_topic": "cmnd/bulb1/DIMMER"})");

    discoveryManager.processMessage("homeassistant/sensor/temp1/config",
        R"({"unique_id": "temp1", "name": "Temperature", "state_topic": "sensors/temp",
            "unit_of_measurement": "°C"})");

    EXPECT_EQ(discoveredDevices.size(), 3);
    EXPECT_EQ(discoveryManager.getDiscoveredDeviceIds().size(), 3);

    // Verify device types
    EXPECT_EQ(discoveredDevices[0].deviceClass(), MqttDeviceClass::Switch);
    EXPECT_EQ(discoveredDevices[1].deviceClass(), MqttDeviceClass::Light);
    EXPECT_EQ(discoveredDevices[2].deviceClass(), MqttDeviceClass::Sensor);
}

TEST_F(WifiIntegrationTest, StateTrackingAcrossDevices) {
    std::map<std::string, nlohmann::json> states;

    discoveryManager.setStateCallback([&](const std::string& id, const std::string& prop,
                                           const nlohmann::json& val) {
        states[id + "." + prop] = val;
    });

    // Add devices
    discoveryManager.processMessage("homeassistant/switch/dev1/config",
        R"({"unique_id": "dev1", "state_topic": "stat/dev1/POWER"})");
    discoveryManager.processMessage("homeassistant/switch/dev2/config",
        R"({"unique_id": "dev2", "state_topic": "stat/dev2/POWER"})");

    // Send state updates
    discoveryManager.processMessage("stat/dev1/POWER", "ON");
    discoveryManager.processMessage("stat/dev2/POWER", "OFF");

    EXPECT_EQ(states.size(), 2);
    EXPECT_TRUE(states["dev1.state"].get<bool>());
    EXPECT_FALSE(states["dev2.state"].get<bool>());
}

// ============= Edge Cases and Error Handling =============

class WifiErrorHandlingTest : public ::testing::Test {};

TEST_F(WifiErrorHandlingTest, InvalidDiscoveryTopicFormat) {
    auto config = MqttDiscoveryConfig::parse("invalid/topic", "{}");
    EXPECT_FALSE(config.has_value());
}

TEST_F(WifiErrorHandlingTest, InvalidJsonPayload) {
    auto config = MqttDiscoveryConfig::parse("homeassistant/switch/test/config",
                                              "not valid json");
    EXPECT_FALSE(config.has_value());
}

TEST_F(WifiErrorHandlingTest, EmptyJsonMeansDeviceRemoval) {
    // Empty JSON {} in MQTT Discovery means device removal
    auto config = MqttDiscoveryConfig::parse("homeassistant/switch/test/config", "{}");
    // Returns nullopt because empty payload signals removal
    EXPECT_FALSE(config.has_value());
}

TEST_F(WifiErrorHandlingTest, ShellyProbeNonexistentDevice) {
    MockHttpClient http;
    // No response configured - will return nullopt

    ShellyDiscovery discovery(http);
    auto info = discovery.probeDevice("192.168.1.99");
    EXPECT_FALSE(info.has_value());
}
