/**
 * RpmsgClient Unit Tests
 *
 * Tests RPMsg client functionality. Hardware tests are skipped
 * when RPMsg device is not available (non-target environment).
 */

#include <gtest/gtest.h>
#include <smarthub_protocol/rpmsg_test_vectors.hpp>
#include <chrono>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
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
    EXPECT_FALSE(client.sendMessage(smarthub::protocol::MsgType::CMD_PING, payload));
}

TEST_F(RpmsgClientTest, PingFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.ping());
}

TEST_F(RpmsgClientTest, RequestSensorDataFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.requestSensorData());
}

TEST_F(RpmsgClientTest, SetGpioFailsWhenNotConnected) {
    smarthub::RpmsgClient client(*eventBus);
    EXPECT_FALSE(client.setGpio(0, 0, true));
    EXPECT_FALSE(client.setGpio(1, 7, false));
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
        // M4 firmware may not be loaded, in which case the device stays open if it exists
        EXPECT_FALSE(client.m4Responded());
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

// ---- The contract with the firmware, whose tests check the same bytes ----

namespace {

/** Plays the M4's end of the device, over a pseudo-terminal, which is what the real one looks like. */
class FakeM4 {
public:
    FakeM4() {
        m_master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        grantpt(m_master);
        unlockpt(m_master);
        m_path = ptsname(m_master);
        // Bytes as they are, or the terminal would alter them before the client opens it
        struct termios tio;
        tcgetattr(m_master, &tio);
        cfmakeraw(&tio);
        tcsetattr(m_master, TCSANOW, &tio);
    }
    ~FakeM4() { hangUp(); }

    const std::string& path() const { return m_path; }

    /** What Linux does to the device when the M4 stops */
    void hangUp() {
        if (m_master >= 0) {
            close(m_master);
            m_master = -1;
        }
    }

    void send(const std::vector<uint8_t>& bytes) {
        ASSERT_EQ(write(m_master, bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
    }

    std::vector<uint8_t> received() {
        std::vector<uint8_t> bytes;
        uint8_t buffer[512];
        for (int i = 0; i < 100; i++) {
            const ssize_t n = read(m_master, buffer, sizeof(buffer));
            if (n > 0) {
                bytes.insert(bytes.end(), buffer, buffer + n);
                return bytes;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return bytes;
    }

private:
    int m_master = -1;
    std::string m_path;
};

} // namespace

class RpmsgContractTest : public RpmsgClientTest {
protected:
    // Answers the first ping, so the client is up
    void connect(smarthub::RpmsgClient& client) {
        std::thread answer([this] {
            EXPECT_EQ(m4.received(), smarthub::protocol::vectors::PING);
            m4.send(smarthub::protocol::vectors::PONG);
        });
        EXPECT_TRUE(client.initialize(2000));
        answer.join();
    }
    FakeM4 m4;
};

TEST_F(RpmsgContractTest, InitializeIsOnlyTrueIfTheM4Answers) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    EXPECT_FALSE(client.initialize(50));
    EXPECT_TRUE(client.isConnected()) << "it stays open in case the M4 starts later";
    EXPECT_FALSE(client.m4Responded());

    m4.send(smarthub::protocol::vectors::PONG);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();
    EXPECT_TRUE(client.m4Responded());
}

TEST_F(RpmsgContractTest, ClientSendsWhatTheFirmwareExpects) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);

    EXPECT_TRUE(client.setGpio(1, 7, true));
    EXPECT_EQ(m4.received(), smarthub::protocol::vectors::SET_GPIO_B7_HIGH);

    EXPECT_TRUE(client.setSensorInterval(1000));
    EXPECT_EQ(m4.received(), smarthub::protocol::vectors::SET_INTERVAL_1000);
}

TEST_F(RpmsgContractTest, ClientReadsWhatTheFirmwareSends) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);

    std::vector<smarthub::SensorDataEvent> events;
    eventBus->subscribe("sensor.data", [&](const smarthub::Event& event) {
        events.push_back(static_cast<const smarthub::SensorDataEvent&>(event));
    });

    m4.send(smarthub::protocol::vectors::SENSOR_UPDATE_21_50);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();
    eventBus->processQueue();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].sensorId, "3");
    EXPECT_DOUBLE_EQ(events[0].value, 21.5);
}

TEST_F(RpmsgContractTest, ClientReassemblesAMessageInPieces) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    int messages = 0;
    client.setMessageCallback([&](const std::vector<uint8_t>&) { messages++; });

    const auto& whole = smarthub::protocol::vectors::SENSOR_UPDATE_21_50;
    m4.send({whole.begin(), whole.begin() + 5});
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();
    EXPECT_EQ(messages, 0);

    m4.send({whole.begin() + 5, whole.end()});
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();
    EXPECT_EQ(messages, 1);
}

TEST_F(RpmsgContractTest, ClientReadsTwoMessagesThatArriveTogether) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    int messages = 0;
    client.setMessageCallback([&](const std::vector<uint8_t>&) { messages++; });

    auto both = smarthub::protocol::vectors::PONG;
    both.insert(both.end(), smarthub::protocol::vectors::SENSOR_UPDATE_21_50.begin(),
                smarthub::protocol::vectors::SENSOR_UPDATE_21_50.end());
    m4.send(both);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();

    EXPECT_EQ(messages, 2);
}

TEST_F(RpmsgContractTest, GarbageIsSkippedAndWhatFollowsIsRead) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    int messages = 0;
    client.setMessageCallback([&](const std::vector<uint8_t>&) { messages++; });

    // Too large, and then a type that doesn't exist
    std::vector<uint8_t> bytes = {0x01, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0x77, 0, 0, 0, 0, 0, 0, 0};
    bytes.insert(bytes.end(), smarthub::protocol::vectors::PONG.begin(), smarthub::protocol::vectors::PONG.end());
    m4.send(bytes);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();

    EXPECT_GE(messages, 1) << "the pong behind the garbage";
}

TEST_F(RpmsgContractTest, MessageLeftUnfinishedDoesNotSpoilTheNext) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    int messages = 0;
    client.setMessageCallback([&](const std::vector<uint8_t>&) { messages++; });

    // The M4 reset partway through a message
    const auto& whole = smarthub::protocol::vectors::SENSOR_UPDATE_21_50;
    m4.send({whole.begin(), whole.begin() + 9});
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    m4.send(smarthub::protocol::vectors::PONG);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();

    EXPECT_EQ(messages, 1);
}

TEST_F(RpmsgContractTest, HandlerCanShutTheClientDown) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    client.setMessageCallback([&](const std::vector<uint8_t>&) { client.shutdown(); });

    auto both = smarthub::protocol::vectors::PONG;
    both.insert(both.end(), smarthub::protocol::vectors::PONG.begin(), smarthub::protocol::vectors::PONG.end());
    m4.send(both);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();

    EXPECT_FALSE(client.isConnected());
}

TEST_F(RpmsgContractTest, RetryingInitializeWaitsForItsOwnAnswer) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    EXPECT_TRUE(client.m4Responded());

    EXPECT_FALSE(client.initialize(30)) << "nothing answered this time";
    EXPECT_FALSE(client.m4Responded());
}

TEST_F(RpmsgContractTest, DeviceComesBackAfterTheM4Restarts) {
    const fs::path link = fs::temp_directory_path() / ("rpmsg_test_" + std::to_string(getpid()));
    fs::remove(link);
    fs::create_symlink(m4.path(), link);

    smarthub::RpmsgClient client(*eventBus, link.string());
    client.setRetryInterval(std::chrono::milliseconds(10));
    connect(client);

    m4.hangUp();
    fs::remove(link);
    for (int i = 0; i < 100 && client.isConnected(); i++) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_FALSE(client.isConnected());
    EXPECT_FALSE(client.m4Responded());
    client.poll();  // With the device gone, this keeps trying and doesn't crash

    FakeM4 restarted;
    fs::create_symlink(restarted.path(), link);
    for (int i = 0; i < 100 && !client.isConnected(); i++) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(client.isConnected());
    const auto sent = restarted.received();  // The sequence number has moved on from the vector's
    ASSERT_EQ(sent.size(), smarthub::protocol::vectors::PING.size()) << "it should ping as soon as it's back";
    EXPECT_EQ(sent[0], smarthub::protocol::vectors::PING[0]);
    restarted.send(smarthub::protocol::vectors::PONG);
    for (int i = 0; i < 100 && !client.m4Responded(); i++) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_TRUE(client.m4Responded());
    fs::remove(link);
}

TEST_F(RpmsgContractTest, ShutdownStopsItComingBack) {
    const fs::path link = fs::temp_directory_path() / ("rpmsg_test_" + std::to_string(getpid()));
    fs::remove(link);
    fs::create_symlink(m4.path(), link);
    smarthub::RpmsgClient client(*eventBus, link.string());
    client.setRetryInterval(std::chrono::milliseconds(1));
    connect(client);

    client.shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    client.poll();
    EXPECT_FALSE(client.isConnected());
    fs::remove(link);
}

TEST_F(RpmsgContractTest, SensorEventSaysWhatItMeasures) {
    smarthub::RpmsgClient client(*eventBus, m4.path());
    connect(client);
    std::string type;
    std::string unit;
    eventBus->subscribe("sensor.data", [&](const smarthub::Event& event) {
        type = static_cast<const smarthub::SensorDataEvent&>(event).sensorType;
        unit = static_cast<const smarthub::SensorDataEvent&>(event).unit;
    });

    m4.send(smarthub::protocol::vectors::SENSOR_UPDATE_21_50);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    client.poll();
    eventBus->processQueue();

    EXPECT_EQ(type, "temperature");
    EXPECT_EQ(unit, "C");
}
