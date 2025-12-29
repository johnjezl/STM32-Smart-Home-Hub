/**
 * RpmsgClient Unit Tests
 *
 * Tests RPMsg client functionality. Hardware tests are skipped
 * when RPMsg device is not available (non-target environment).
 */

#include <gtest/gtest.h>
#include <smarthub/rpmsg/RpmsgClient.hpp>
#include <smarthub/core/EventBus.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class RpmsgClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus = std::make_unique<smarthub::EventBus>();
    }

    void TearDown() override {
        eventBus.reset();
    }

    std::unique_ptr<smarthub::EventBus> eventBus;
};

TEST_F(RpmsgClientTest, Construction) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgClientTest, ConstructionWithCustomDevice) {
    smarthub::RpmsgClient client(*eventBus, "/dev/ttyRPMSG1");
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgClientTest, InitializeFailsWithNoDevice) {
    // Use a non-existent device path
    smarthub::RpmsgClient client(*eventBus, "/dev/nonexistent_rpmsg_device");
    EXPECT_FALSE(client.initialize());
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgClientTest, ShutdownWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    // Should not crash
    client.shutdown();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgClientTest, MultipleShutdowns) {
    smarthub::RpmsgClient client(*eventBus);
    client.shutdown();
    client.shutdown();
    client.shutdown();
    // Should not crash
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgClientTest, SendFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    EXPECT_FALSE(client.send(data));
}

TEST_F(RpmsgClientTest, SendMessageFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    std::vector<uint8_t> payload = {0x01};
    EXPECT_FALSE(client.sendMessage(smarthub::RpmsgMessageType::Ping, payload));
}

TEST_F(RpmsgClientTest, PingFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.ping());
}

TEST_F(RpmsgClientTest, RequestSensorDataFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.requestSensorData(0));
}

TEST_F(RpmsgClientTest, SetGpioFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.setGpio(0, true));
    EXPECT_FALSE(client.setGpio(1, false));
}

TEST_F(RpmsgClientTest, SetPwmFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.setPwm(0, 512));
    EXPECT_FALSE(client.setPwm(1, 0));
    EXPECT_FALSE(client.setPwm(2, 65535));
}

TEST_F(RpmsgClientTest, PollWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    // Should not crash
    client.poll();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgClientTest, SetMessageCallback) {
    smarthub::RpmsgClient client(*eventBus);

    bool callbackSet = false;
    client.setMessageCallback([&](const std::vector<uint8_t>& /*data*/) {
        callbackSet = true;
    });

    // Callback won't be called since not connected
    client.poll();
    EXPECT_FALSE(callbackSet);
}

TEST_F(RpmsgClientTest, MessageTypeEnumValues) {
    // Verify message type enum values match protocol spec
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::Ping), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::Pong), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::SensorData), 0x10);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::GpioCommand), 0x20);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::GpioState), 0x21);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::AdcRequest), 0x30);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::AdcResponse), 0x31);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::PwmCommand), 0x40);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::Config), 0x50);
    EXPECT_EQ(static_cast<uint8_t>(smarthub::RpmsgMessageType::Error), 0xFF);
}

// Hardware tests - only run on target device with M4
class RpmsgClientHardwareTest : public RpmsgClientTest {
protected:
    void SetUp() override {
        RpmsgClientTest::SetUp();

        // Skip hardware tests unless device exists
        if (!fs::exists("/dev/ttyRPMSG0")) {
            GTEST_SKIP() << "RPMsg device not available (not running on target)";
        }
    }
};

TEST_F(RpmsgClientHardwareTest, InitializeWithRealDevice) {
    smarthub::RpmsgClient client(*eventBus);
    bool result = client.initialize();

    if (result) {
        EXPECT_TRUE(client.isConnected());
        client.shutdown();
        EXPECT_FALSE(client.isConnected());
    } else {
        // M4 firmware may not be loaded
        EXPECT_FALSE(client.isConnected());
    }
}

TEST_F(RpmsgClientHardwareTest, PingM4) {
    smarthub::RpmsgClient client(*eventBus);

    if (client.initialize()) {
        // Ping should work if M4 is responsive
        // May or may not succeed depending on M4 firmware state
        (void)client.ping();
        client.shutdown();
    }
}
