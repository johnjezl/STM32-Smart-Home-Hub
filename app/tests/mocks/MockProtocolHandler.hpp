/**
 * Mock Protocol Handler for Testing
 */
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>
#include <smarthub/protocols/ProtocolFactory.hpp>

namespace smarthub::testing {

/**
 * Mock protocol handler for unit testing
 */
class MockProtocolHandler : public IProtocolHandler {
public:
    MockProtocolHandler(EventBus& /* eventBus */, const nlohmann::json& /* config */ = {}) {}

    // IProtocolHandler implementation
    std::string name() const override { return "mock"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "Mock protocol for testing"; }

    bool initialize() override {
        initialized = true;
        m_state = ProtocolState::Connected;
        return initializeResult;
    }

    void shutdown() override {
        initialized = false;
        m_state = ProtocolState::Disconnected;
    }

    void poll() override {
        pollCount++;
    }

    ProtocolState state() const override { return m_state; }
    bool isConnected() const override { return m_state == ProtocolState::Connected; }
    std::string lastError() const override { return m_lastError; }

    bool supportsDiscovery() const override { return true; }

    void startDiscovery() override {
        discovering = true;
    }

    void stopDiscovery() override {
        discovering = false;
    }

    bool isDiscovering() const override { return discovering; }

    bool sendCommand(const std::string& address, const std::string& command,
                     const nlohmann::json& params) override {
        lastCommandAddress = address;
        lastCommand = command;
        lastParams = params;
        commandCount++;
        return commandResult;
    }

    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override {
        discoveredCallback = cb;
    }

    void setDeviceStateCallback(DeviceStateCallback cb) override {
        stateCallback = cb;
    }

    void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) override {
        availabilityCallback = cb;
    }

    nlohmann::json getStatus() const override {
        return {
            {"connected", isConnected()},
            {"discovering", discovering},
            {"commandCount", commandCount}
        };
    }

    std::vector<std::string> getKnownDeviceAddresses() const override {
        return knownAddresses;
    }

    // Test helpers
    void simulateDeviceDiscovered(DevicePtr device) {
        if (discoveredCallback) {
            discoveredCallback(device);
        }
    }

    void simulateStateChange(const std::string& id, const std::string& prop,
                             const nlohmann::json& val) {
        if (stateCallback) {
            stateCallback(id, prop, val);
        }
    }

    void simulateAvailabilityChange(const std::string& id, DeviceAvailability avail) {
        if (availabilityCallback) {
            availabilityCallback(id, avail);
        }
    }

    void setError(const std::string& error) {
        m_lastError = error;
        m_state = ProtocolState::Error;
    }

    // State for verification
    bool initialized = false;
    bool initializeResult = true;
    bool discovering = false;
    bool commandResult = true;
    int pollCount = 0;
    int commandCount = 0;
    std::string lastCommandAddress;
    std::string lastCommand;
    nlohmann::json lastParams;
    std::vector<std::string> knownAddresses;

private:
    ProtocolState m_state = ProtocolState::Disconnected;
    std::string m_lastError;

    DeviceDiscoveredCallback discoveredCallback;
    DeviceStateCallback stateCallback;
    DeviceAvailabilityCallback availabilityCallback;
};

/**
 * Register mock protocol for testing
 * Call this in test setup if needed
 */
inline void registerMockProtocol() {
    ProtocolFactory::instance().registerProtocol(
        "mock",
        [](EventBus& eb, const nlohmann::json& cfg) -> ProtocolHandlerPtr {
            return std::make_shared<MockProtocolHandler>(eb, cfg);
        },
        {"mock", "1.0.0", "Mock protocol for testing"}
    );
}

} // namespace smarthub::testing
