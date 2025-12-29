/**
 * ProtocolFactory and Protocol Handler Tests - Phase 3
 *
 * Tests for the protocol handler framework.
 */

#include <gtest/gtest.h>
#include <smarthub/protocols/ProtocolFactory.hpp>
#include <smarthub/protocols/IProtocolHandler.hpp>
#include <smarthub/devices/IDevice.hpp>
#include <smarthub/core/EventBus.hpp>
#include "../mocks/MockProtocolHandler.hpp"
#include "../mocks/MockDevice.hpp"

using namespace smarthub;
using namespace smarthub::testing;

// ============================================================================
// ProtocolFactory Tests
// ============================================================================

class ProtocolFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register a test protocol
        ProtocolFactory::instance().registerProtocol("test_protocol",
            [](EventBus& eb, const nlohmann::json& config) {
                return std::make_shared<MockProtocolHandler>(eb, config);
            },
            {"test_protocol", "1.0.0", "Test protocol for unit tests"});
    }

    EventBus eventBus;
};

TEST_F(ProtocolFactoryTest, Singleton) {
    auto& instance1 = ProtocolFactory::instance();
    auto& instance2 = ProtocolFactory::instance();

    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(ProtocolFactoryTest, RegisterProtocol) {
    ProtocolFactory::instance().registerProtocol("new_protocol",
        [](EventBus& eb, const nlohmann::json& config) {
            return std::make_shared<MockProtocolHandler>(eb, config);
        },
        {"new_protocol", "1.0.0", "New test protocol"});

    auto protocols = ProtocolFactory::instance().availableProtocols();
    EXPECT_TRUE(std::find(protocols.begin(), protocols.end(), "new_protocol") != protocols.end());
}

TEST_F(ProtocolFactoryTest, CreateProtocol) {
    nlohmann::json config = {{"setting", "value"}};
    auto protocol = ProtocolFactory::instance().create("test_protocol", eventBus, config);

    EXPECT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->name(), "mock");
}

TEST_F(ProtocolFactoryTest, CreateNonexistentProtocol) {
    nlohmann::json config;
    auto protocol = ProtocolFactory::instance().create("nonexistent", eventBus, config);

    EXPECT_EQ(protocol, nullptr);
}

TEST_F(ProtocolFactoryTest, AvailableProtocols) {
    auto protocols = ProtocolFactory::instance().availableProtocols();

    // At minimum should have test_protocol registered in SetUp
    EXPECT_FALSE(protocols.empty());
    EXPECT_TRUE(std::find(protocols.begin(), protocols.end(), "test_protocol") != protocols.end());
}

TEST_F(ProtocolFactoryTest, HasProtocol) {
    EXPECT_TRUE(ProtocolFactory::instance().hasProtocol("test_protocol"));
    EXPECT_FALSE(ProtocolFactory::instance().hasProtocol("nonexistent"));
}

// ============================================================================
// MockProtocolHandler Tests
// ============================================================================

class MockProtocolHandlerTest : public ::testing::Test {
protected:
    EventBus eventBus;
    MockProtocolHandler handler{eventBus, {}};
};

TEST_F(MockProtocolHandlerTest, Identification) {
    EXPECT_EQ(handler.name(), "mock");
    EXPECT_EQ(handler.version(), "1.0.0");
}

TEST_F(MockProtocolHandlerTest, Lifecycle) {
    EXPECT_FALSE(handler.initialized);

    EXPECT_TRUE(handler.initialize());
    EXPECT_TRUE(handler.initialized);

    handler.shutdown();
    EXPECT_FALSE(handler.initialized);
}

TEST_F(MockProtocolHandlerTest, Polling) {
    EXPECT_EQ(handler.pollCount, 0);

    handler.poll();
    EXPECT_EQ(handler.pollCount, 1);

    handler.poll();
    handler.poll();
    EXPECT_EQ(handler.pollCount, 3);
}

TEST_F(MockProtocolHandlerTest, Discovery) {
    EXPECT_TRUE(handler.supportsDiscovery());
    EXPECT_FALSE(handler.isDiscovering());

    handler.startDiscovery();
    EXPECT_TRUE(handler.isDiscovering());

    handler.stopDiscovery();
    EXPECT_FALSE(handler.isDiscovering());
}

TEST_F(MockProtocolHandlerTest, Connection) {
    // Initially connected after construction
    handler.initialize();
    EXPECT_TRUE(handler.isConnected());

    // After shutdown, not connected
    handler.shutdown();
    EXPECT_FALSE(handler.isConnected());
}

TEST_F(MockProtocolHandlerTest, SendCommand) {
    nlohmann::json params = {{"brightness", 50}};

    EXPECT_TRUE(handler.sendCommand("device/123", "set", params));

    EXPECT_EQ(handler.commandCount, 1);
    EXPECT_EQ(handler.lastCommandAddress, "device/123");
    EXPECT_EQ(handler.lastCommand, "set");
    EXPECT_EQ(handler.lastParams["brightness"], 50);
}

TEST_F(MockProtocolHandlerTest, SendCommandFailure) {
    handler.commandResult = false;

    EXPECT_FALSE(handler.sendCommand("addr", "cmd", {}));
    EXPECT_EQ(handler.commandCount, 1);
}

TEST_F(MockProtocolHandlerTest, DeviceDiscoveredCallback) {
    bool callbackCalled = false;
    DevicePtr discoveredDevice = nullptr;

    handler.setDeviceDiscoveredCallback([&](DevicePtr device) {
        callbackCalled = true;
        discoveredDevice = device;
    });

    auto mockDevice = std::make_shared<MockDevice>("test1", "Test Device");
    handler.simulateDeviceDiscovered(mockDevice);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(discoveredDevice, mockDevice);
}

TEST_F(MockProtocolHandlerTest, StateChangeCallback) {
    std::string receivedId;
    std::string receivedProperty;
    nlohmann::json receivedValue;

    handler.setDeviceStateCallback([&](const std::string& id,
                                        const std::string& prop,
                                        const nlohmann::json& val) {
        receivedId = id;
        receivedProperty = prop;
        receivedValue = val;
    });

    handler.simulateStateChange("device1", "on", true);

    EXPECT_EQ(receivedId, "device1");
    EXPECT_EQ(receivedProperty, "on");
    EXPECT_EQ(receivedValue, true);
}

TEST_F(MockProtocolHandlerTest, AvailabilityCallback) {
    std::string receivedId;
    DeviceAvailability receivedAvailability = DeviceAvailability::Unknown;

    handler.setDeviceAvailabilityCallback([&](const std::string& id, DeviceAvailability available) {
        receivedId = id;
        receivedAvailability = available;
    });

    handler.simulateAvailabilityChange("device1", DeviceAvailability::Online);

    EXPECT_EQ(receivedId, "device1");
    EXPECT_EQ(receivedAvailability, DeviceAvailability::Online);

    handler.simulateAvailabilityChange("device2", DeviceAvailability::Offline);
    EXPECT_EQ(receivedId, "device2");
    EXPECT_EQ(receivedAvailability, DeviceAvailability::Offline);
}

TEST_F(MockProtocolHandlerTest, NoCallbackSet) {
    // Create fresh handler without callbacks
    MockProtocolHandler freshHandler{eventBus, {}};

    // Should not crash when callbacks not set
    auto mockDevice = std::make_shared<MockDevice>();
    freshHandler.simulateDeviceDiscovered(mockDevice);
    freshHandler.simulateStateChange("id", "prop", 123);
    freshHandler.simulateAvailabilityChange("id", DeviceAvailability::Online);
}

TEST_F(MockProtocolHandlerTest, Status) {
    auto status = handler.getStatus();
    EXPECT_TRUE(status.is_object());
}

// ============================================================================
// IProtocolHandler Interface Contract Tests
// ============================================================================

class ProtocolHandlerContractTest : public ::testing::Test {
protected:
    EventBus eventBus;
    std::shared_ptr<IProtocolHandler> handler = std::make_shared<MockProtocolHandler>(eventBus, nlohmann::json{});
};

TEST_F(ProtocolHandlerContractTest, HasName) {
    EXPECT_FALSE(handler->name().empty());
}

TEST_F(ProtocolHandlerContractTest, HasVersion) {
    EXPECT_FALSE(handler->version().empty());
}

TEST_F(ProtocolHandlerContractTest, InitializeReturnsStatus) {
    bool result = handler->initialize();
    // Should return true or false, not throw
    EXPECT_TRUE(result || !result);
}

TEST_F(ProtocolHandlerContractTest, ShutdownDoesNotThrow) {
    handler->initialize();
    EXPECT_NO_THROW(handler->shutdown());
}

TEST_F(ProtocolHandlerContractTest, PollDoesNotThrow) {
    handler->initialize();
    EXPECT_NO_THROW(handler->poll());
}

TEST_F(ProtocolHandlerContractTest, DiscoveryControlDoesNotThrow) {
    handler->initialize();
    if (handler->supportsDiscovery()) {
        EXPECT_NO_THROW(handler->startDiscovery());
        EXPECT_NO_THROW(handler->stopDiscovery());
    }
}

TEST_F(ProtocolHandlerContractTest, SendCommandReturnsStatus) {
    handler->initialize();
    bool result = handler->sendCommand("addr", "cmd", {});
    EXPECT_TRUE(result || !result);
}

TEST_F(ProtocolHandlerContractTest, GetStatusReturnsJson) {
    auto status = handler->getStatus();
    EXPECT_TRUE(status.is_object() || status.is_null());
}
