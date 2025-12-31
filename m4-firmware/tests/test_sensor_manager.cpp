/**
 * Sensor Manager Tests
 *
 * Tests sensor polling logic and timing.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>

namespace {

/**
 * Mock clock for testing (simulates Clock::getTicks())
 */
class MockClock {
public:
    MockClock() : m_ticks(0) {}

    uint32_t getTicks() const { return m_ticks; }

    void setTicks(uint32_t ticks) { m_ticks = ticks; }

    void advance(uint32_t ms) { m_ticks += ms; }

private:
    uint32_t m_ticks;
};

/**
 * Sensor reading structure
 */
struct SensorReading {
    uint8_t sensorId;
    uint8_t sensorType;  // 1=Temp, 2=Humidity
    int32_t value;       // Fixed-point x100
    uint32_t timestamp;
};

/**
 * Mock sensor for testing
 */
class MockSensor {
public:
    MockSensor(uint8_t id) : m_id(id), m_present(true) {}

    bool isPresent() const { return m_present; }
    void setPresent(bool present) { m_present = present; }

    uint8_t getId() const { return m_id; }

    void setTemperature(float temp) { m_temperature = temp; }
    void setHumidity(float hum) { m_humidity = hum; }

    bool measure() {
        if (!m_present) return false;
        m_measureCount++;
        return true;
    }

    float temperature() const { return m_temperature; }
    float humidity() const { return m_humidity; }

    int32_t temperatureFixed() const { return static_cast<int32_t>(m_temperature * 100); }
    int32_t humidityFixed() const { return static_cast<int32_t>(m_humidity * 100); }

    int measureCount() const { return m_measureCount; }

private:
    uint8_t m_id;
    bool m_present;
    float m_temperature = 25.0f;
    float m_humidity = 50.0f;
    int m_measureCount = 0;
};

/**
 * Sensor manager for testing
 */
class SensorManager {
public:
    static constexpr uint16_t DEFAULT_POLL_INTERVAL = 1000;  // 1 second
    static constexpr uint16_t MIN_POLL_INTERVAL = 100;       // 100ms minimum
    static constexpr uint16_t MAX_POLL_INTERVAL = 60000;     // 60s maximum

    using Callback = std::function<void(const SensorReading&)>;

    SensorManager(MockClock& clock) : m_clock(clock), m_pollInterval(DEFAULT_POLL_INTERVAL) {}

    void addSensor(MockSensor* sensor) {
        m_sensors.push_back(sensor);
    }

    void setCallback(Callback cb) {
        m_callback = cb;
    }

    void setPollInterval(uint32_t intervalMs) {
        if (intervalMs < MIN_POLL_INTERVAL) {
            m_pollInterval = MIN_POLL_INTERVAL;
        } else if (intervalMs > MAX_POLL_INTERVAL) {
            m_pollInterval = MAX_POLL_INTERVAL;
        } else {
            m_pollInterval = static_cast<uint16_t>(intervalMs);
        }
    }

    uint16_t getPollInterval() const { return m_pollInterval; }

    void poll() {
        uint32_t now = m_clock.getTicks();

        // Check if it's time to poll (first poll always triggers)
        if (m_firstPoll) {
            m_firstPoll = false;
            m_lastPoll = now;
        } else if (now - m_lastPoll < m_pollInterval) {
            return;
        } else {
            m_lastPoll = now;
        }

        // Poll all sensors
        for (auto* sensor : m_sensors) {
            if (!sensor->isPresent()) continue;

            if (sensor->measure()) {
                // Report temperature
                if (m_callback) {
                    SensorReading tempReading;
                    tempReading.sensorId = sensor->getId();
                    tempReading.sensorType = 1;  // Temperature
                    tempReading.value = sensor->temperatureFixed();
                    tempReading.timestamp = now;
                    m_callback(tempReading);

                    // Report humidity
                    SensorReading humReading;
                    humReading.sensorId = sensor->getId();
                    humReading.sensorType = 2;  // Humidity
                    humReading.value = sensor->humidityFixed();
                    humReading.timestamp = now;
                    m_callback(humReading);
                }
            }
        }
    }

    size_t sensorCount() const { return m_sensors.size(); }

    size_t activeSensorCount() const {
        size_t count = 0;
        for (auto* sensor : m_sensors) {
            if (sensor->isPresent()) count++;
        }
        return count;
    }

private:
    MockClock& m_clock;
    std::vector<MockSensor*> m_sensors;
    Callback m_callback;
    uint16_t m_pollInterval;
    uint32_t m_lastPoll = 0;
    bool m_firstPoll = true;
};

} // anonymous namespace

class SensorManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        clock = std::make_unique<MockClock>();
        manager = std::make_unique<SensorManager>(*clock);
    }

    std::unique_ptr<MockClock> clock;
    std::unique_ptr<SensorManager> manager;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(SensorManagerTest, DefaultPollInterval) {
    EXPECT_EQ(manager->getPollInterval(), 1000);
}

TEST_F(SensorManagerTest, NoSensorsInitially) {
    EXPECT_EQ(manager->sensorCount(), 0);
    EXPECT_EQ(manager->activeSensorCount(), 0);
}

TEST_F(SensorManagerTest, AddSensor) {
    MockSensor sensor1(0);
    MockSensor sensor2(1);

    manager->addSensor(&sensor1);
    EXPECT_EQ(manager->sensorCount(), 1);

    manager->addSensor(&sensor2);
    EXPECT_EQ(manager->sensorCount(), 2);
}

// ============================================================================
// Poll Interval Tests
// ============================================================================

TEST_F(SensorManagerTest, SetPollInterval) {
    manager->setPollInterval(500);
    EXPECT_EQ(manager->getPollInterval(), 500);

    manager->setPollInterval(2000);
    EXPECT_EQ(manager->getPollInterval(), 2000);
}

TEST_F(SensorManagerTest, PollIntervalMinClamp) {
    manager->setPollInterval(50);  // Below minimum
    EXPECT_EQ(manager->getPollInterval(), SensorManager::MIN_POLL_INTERVAL);
}

TEST_F(SensorManagerTest, PollIntervalMaxClamp) {
    manager->setPollInterval(65000);  // Above maximum (60000), within uint16_t range
    EXPECT_EQ(manager->getPollInterval(), SensorManager::MAX_POLL_INTERVAL);
}

TEST_F(SensorManagerTest, PollIntervalAtBoundaries) {
    manager->setPollInterval(100);  // Exactly minimum
    EXPECT_EQ(manager->getPollInterval(), 100);

    manager->setPollInterval(60000);  // Exactly maximum
    EXPECT_EQ(manager->getPollInterval(), 60000);
}

// ============================================================================
// Polling Timing Tests
// ============================================================================

TEST_F(SensorManagerTest, FirstPollImmediate) {
    MockSensor sensor(0);
    sensor.setTemperature(25.0f);
    sensor.setHumidity(50.0f);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    // First poll should happen immediately
    clock->setTicks(0);
    manager->poll();
    EXPECT_EQ(readings.size(), 2);  // Temp + humidity
}

TEST_F(SensorManagerTest, PollAtInterval) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);
    manager->setPollInterval(1000);

    int pollCount = 0;
    manager->setCallback([&](const SensorReading&) {
        pollCount++;
    });

    clock->setTicks(0);
    manager->poll();  // First poll
    int initialCount = pollCount;

    // 500ms later - should not poll
    clock->setTicks(500);
    manager->poll();
    EXPECT_EQ(pollCount, initialCount);

    // 999ms later - should not poll
    clock->setTicks(999);
    manager->poll();
    EXPECT_EQ(pollCount, initialCount);

    // 1000ms later - should poll
    clock->setTicks(1000);
    manager->poll();
    EXPECT_GT(pollCount, initialCount);
}

TEST_F(SensorManagerTest, MultiplePollCycles) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);
    manager->setPollInterval(1000);

    int pollCount = 0;
    manager->setCallback([&](const SensorReading&) {
        pollCount++;
    });

    // Poll at t=0, t=1000, t=2000, t=3000, t=4000
    for (int i = 0; i <= 4; i++) {
        clock->setTicks(i * 1000);
        manager->poll();
    }

    // 5 poll cycles, 2 readings each (temp + humidity) = 10 readings
    EXPECT_EQ(pollCount, 10);
}

TEST_F(SensorManagerTest, FastPollInterval) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);
    manager->setPollInterval(100);  // 100ms

    int pollCount = 0;
    manager->setCallback([&](const SensorReading&) {
        pollCount++;
    });

    // Poll every 100ms for 1 second (11 times: 0, 100, 200, ... 1000)
    for (int i = 0; i <= 1000; i += 100) {
        clock->setTicks(i);
        manager->poll();
    }

    // 11 poll cycles, 2 readings each = 22 readings
    EXPECT_EQ(pollCount, 22);
}

// ============================================================================
// Sensor Reading Tests
// ============================================================================

TEST_F(SensorManagerTest, ReadingsContainCorrectValues) {
    MockSensor sensor(0);
    sensor.setTemperature(23.45f);
    sensor.setHumidity(67.89f);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    clock->setTicks(5000);
    manager->poll();

    ASSERT_EQ(readings.size(), 2);

    // Temperature reading
    EXPECT_EQ(readings[0].sensorId, 0);
    EXPECT_EQ(readings[0].sensorType, 1);
    EXPECT_EQ(readings[0].value, 2345);
    EXPECT_EQ(readings[0].timestamp, 5000u);

    // Humidity reading
    EXPECT_EQ(readings[1].sensorId, 0);
    EXPECT_EQ(readings[1].sensorType, 2);
    EXPECT_EQ(readings[1].value, 6789);
    EXPECT_EQ(readings[1].timestamp, 5000u);
}

TEST_F(SensorManagerTest, MultipleSensors) {
    MockSensor sensor1(0);
    sensor1.setTemperature(20.0f);
    sensor1.setHumidity(40.0f);

    MockSensor sensor2(1);
    sensor2.setTemperature(25.0f);
    sensor2.setHumidity(60.0f);

    manager->addSensor(&sensor1);
    manager->addSensor(&sensor2);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    manager->poll();

    EXPECT_EQ(readings.size(), 4);  // 2 sensors x 2 readings each

    // Sensor 1 temp
    EXPECT_EQ(readings[0].sensorId, 0);
    EXPECT_EQ(readings[0].value, 2000);

    // Sensor 1 humidity
    EXPECT_EQ(readings[1].sensorId, 0);
    EXPECT_EQ(readings[1].value, 4000);

    // Sensor 2 temp
    EXPECT_EQ(readings[2].sensorId, 1);
    EXPECT_EQ(readings[2].value, 2500);

    // Sensor 2 humidity
    EXPECT_EQ(readings[3].sensorId, 1);
    EXPECT_EQ(readings[3].value, 6000);
}

// ============================================================================
// Sensor Presence Tests
// ============================================================================

TEST_F(SensorManagerTest, SensorNotPresent) {
    MockSensor sensor(0);
    sensor.setPresent(false);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    manager->poll();

    EXPECT_TRUE(readings.empty());
    EXPECT_EQ(sensor.measureCount(), 0);
}

TEST_F(SensorManagerTest, SensorBecomesPresent) {
    MockSensor sensor(0);
    sensor.setPresent(false);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    // Not present
    clock->setTicks(0);
    manager->poll();
    EXPECT_TRUE(readings.empty());

    // Becomes present
    sensor.setPresent(true);
    clock->setTicks(1000);
    manager->poll();
    EXPECT_EQ(readings.size(), 2);
}

TEST_F(SensorManagerTest, ActiveSensorCount) {
    MockSensor sensor1(0);
    MockSensor sensor2(1);
    MockSensor sensor3(2);

    sensor1.setPresent(true);
    sensor2.setPresent(false);
    sensor3.setPresent(true);

    manager->addSensor(&sensor1);
    manager->addSensor(&sensor2);
    manager->addSensor(&sensor3);

    EXPECT_EQ(manager->sensorCount(), 3);
    EXPECT_EQ(manager->activeSensorCount(), 2);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SensorManagerTest, NegativeTemperature) {
    MockSensor sensor(0);
    sensor.setTemperature(-10.5f);
    sensor.setHumidity(80.0f);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    manager->poll();

    EXPECT_EQ(readings[0].value, -1050);  // -10.50 * 100
}

TEST_F(SensorManagerTest, ExtremeTemperatures) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    // Very cold
    sensor.setTemperature(-40.0f);
    clock->setTicks(0);
    manager->poll();
    EXPECT_EQ(readings[0].value, -4000);

    readings.clear();

    // Very hot
    sensor.setTemperature(85.0f);
    clock->setTicks(1000);
    manager->poll();
    EXPECT_EQ(readings[0].value, 8500);
}

TEST_F(SensorManagerTest, NoCallback) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);

    // Don't set callback - should not crash
    manager->poll();
    EXPECT_EQ(sensor.measureCount(), 1);
}

TEST_F(SensorManagerTest, CallbackUpdatesReadings) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);

    std::vector<SensorReading> readings;
    manager->setCallback([&](const SensorReading& r) {
        readings.push_back(r);
    });

    // First reading
    sensor.setTemperature(20.0f);
    clock->setTicks(0);
    manager->poll();

    // Update sensor and poll again
    sensor.setTemperature(25.0f);
    clock->setTicks(1000);
    manager->poll();

    // Should have 4 readings total
    EXPECT_EQ(readings.size(), 4);
    EXPECT_EQ(readings[0].value, 2000);  // First temp
    EXPECT_EQ(readings[2].value, 2500);  // Second temp
}

// ============================================================================
// Timing Wrap-around Tests
// ============================================================================

TEST_F(SensorManagerTest, TimerWrapAround) {
    MockSensor sensor(0);
    manager->addSensor(&sensor);
    manager->setPollInterval(1000);

    int pollCount = 0;
    manager->setCallback([&](const SensorReading&) {
        pollCount++;
    });

    // Start near max uint32
    clock->setTicks(0xFFFFFFF0);
    manager->poll();
    int countBefore = pollCount;

    // Wrap around
    clock->setTicks(0x000003E8);  // 1000 ticks after wrap
    manager->poll();

    // Should have polled (timer arithmetic handles wrap)
    EXPECT_GT(pollCount, countBefore);
}
