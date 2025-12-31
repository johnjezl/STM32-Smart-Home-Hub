# SmartHub Testing Guide

## Overview

SmartHub uses **Google Test (gtest)** for unit and integration testing, with **CMake/CTest** for test orchestration and **lcov** for code coverage analysis.

## Quick Start

### Running All Tests

```bash
# From project root
./tools/test.sh

# Or manually
cd app
cmake -B build -DSMARTHUB_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

### Running Tests with Coverage

```bash
./tools/test.sh coverage
# Coverage report: app/build-coverage/coverage_html/index.html
```

### Running Specific Tests

```bash
# Run only Logger tests
./tools/test.sh Logger

# Run with verbose output
./tools/test.sh verbose

# Run specific test with verbose
ctest -R "EventBus" --verbose
```

## Test Structure

```
app/tests/
├── CMakeLists.txt              # Test build configuration
├── main.cpp                    # Main test runner entry point
├── core/
│   ├── test_logger.cpp         # Logger unit tests
│   ├── test_config.cpp         # Config unit tests
│   └── test_eventbus.cpp       # EventBus unit tests
├── database/
│   └── test_database.cpp       # Database unit tests
├── devices/
│   ├── test_device.cpp         # Device and device types unit tests
│   ├── test_device_manager.cpp # DeviceManager unit tests
│   └── test_organization.cpp   # Room and DeviceGroup tests
├── protocols/
│   ├── test_mqtt.cpp           # MQTT client tests
│   ├── test_protocol_factory.cpp # ProtocolFactory and handler tests
│   ├── test_zigbee.cpp         # Zigbee protocol tests (42 cases)
│   └── test_wifi.cpp           # WiFi protocol tests (43 cases)
├── rpmsg/
│   └── test_rpmsg.cpp          # RPMsg M4 communication tests
├── web/
│   └── test_webserver.cpp      # WebServer REST API tests
├── ui/
│   └── test_ui.cpp             # UI manager DRM backend tests
├── mocks/
│   ├── MockDevice.hpp          # Mock device implementations
│   └── MockProtocolHandler.hpp # Mock protocol handler
└── integration/
    └── test_integration.cpp    # Integration tests
```

## Test Categories

### Unit Tests

Unit tests verify individual components in isolation:

| Test Suite | File | Description |
|------------|------|-------------|
| Logger | `core/test_logger.cpp` | Logging levels, file output, formatting |
| Config | `core/test_config.cpp` | YAML loading, INI fallback parser, default values, getters |
| EventBus | `core/test_eventbus.cpp` | Pub/sub, async events, unsubscribe |
| Database | `database/test_database.cpp` | SQLite operations, schema, transactions |
| Device | `devices/test_device.cpp` | Device types (Switch, Dimmer, ColorLight, Sensors), state, callbacks |
| DeviceManager | `devices/test_device_manager.cpp` | Add/remove, queries by type/room/protocol |
| Organization | `devices/test_organization.cpp` | Room and DeviceGroup management |
| WebServer | `web/test_webserver.cpp` | HTTP server, REST API, concurrent requests |
| MQTT | `protocols/test_mqtt.cpp` | MQTT client, subscribe/publish, callbacks |
| ProtocolFactory | `protocols/test_protocol_factory.cpp` | Protocol registration, creation, mock handler |
| Zigbee | `protocols/test_zigbee.cpp` | ZNP frames, transport, coordinator, handler, database |
| WiFi | `protocols/test_wifi.cpp` | MQTT Discovery, Shelly, Tuya, WifiHandler |
| RPMsg | `rpmsg/test_rpmsg.cpp` | M4 communication, message types, GPIO/PWM |
| UI | `ui/test_ui.cpp` | UIManager DRM backend, graceful failure handling |

### Integration Tests

Integration tests verify multiple components working together:

| Test | Description |
|------|-------------|
| ConfigLoadAndDatabaseInit | Config loads, database opens at configured path |
| DeviceManagerWithEventBus | Device operations trigger correct events |
| DevicePersistenceToDatabase | Devices can be stored and retrieved |
| EventBusAsyncProcessing | Async events are queued and processed |
| SensorDataLogging | Sensor readings are stored and queryable |

## Writing New Tests

### Test File Template

```cpp
/**
 * ComponentName Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/module/ComponentName.hpp>

class ComponentNameTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup before each test
    }

    void TearDown() override {
        // Cleanup after each test
    }

    // Test fixtures
    smarthub::EventBus eventBus;
};

TEST_F(ComponentNameTest, FeatureWorks) {
    // Arrange
    ComponentName component;

    // Act
    auto result = component.doSomething();

    // Assert
    EXPECT_EQ(result, expectedValue);
}

TEST_F(ComponentNameTest, HandlesEdgeCase) {
    ComponentName component;
    EXPECT_THROW(component.invalidOperation(), std::exception);
}
```

### Adding to Build

1. Create test file in appropriate directory
2. Add to `tests/CMakeLists.txt`:

```cmake
# Individual test executable
add_executable(test_newcomponent newcomponent/test_newcomponent.cpp)
target_link_libraries(test_newcomponent PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
add_test(NAME NewComponent COMMAND test_newcomponent)

# Also add to all-in-one runner
add_executable(smarthub_tests
    main.cpp
    # ... existing tests ...
    newcomponent/test_newcomponent.cpp
)
```

### Assertion Reference

```cpp
// Boolean
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Equality
EXPECT_EQ(expected, actual);
EXPECT_NE(expected, actual);

// Comparisons
EXPECT_LT(a, b);   // a < b
EXPECT_LE(a, b);   // a <= b
EXPECT_GT(a, b);   // a > b
EXPECT_GE(a, b);   // a >= b

// Floating point
EXPECT_NEAR(expected, actual, tolerance);

// Strings
EXPECT_STREQ(expected, actual);
EXPECT_STRNE(expected, actual);

// Pointers
EXPECT_EQ(ptr, nullptr);
EXPECT_NE(ptr, nullptr);

// Exceptions
EXPECT_THROW(statement, exception_type);
EXPECT_NO_THROW(statement);
```

Use `ASSERT_*` instead of `EXPECT_*` when the test cannot continue if the assertion fails.

## CI/CD Pipeline

The GitHub Actions workflow (`.github/workflows/ci.yml`) runs:

1. **Native Build & Test** - Full build and test on x86_64 Linux
2. **ARM Cross-compile** - Verify ARM32 cross-compilation
3. **Code Coverage** - Generate coverage report
4. **Static Analysis** - cppcheck and clang-tidy

### Triggering CI

- Push to `main` or `develop` branch
- Pull request to `main`
- Manual trigger via GitHub Actions UI

### Viewing Results

- Test results: Check Actions tab in GitHub
- Coverage: Download artifact from workflow run
- Artifacts retained for 7 days (30 days for coverage)

## Code Coverage

### Local Coverage Report

```bash
./tools/test.sh coverage
# Open app/build-coverage/coverage_html/index.html in browser
```

### Coverage Goals

| Module | Target Coverage |
|--------|-----------------|
| Core (Logger, EventBus, Config) | 80%+ |
| Database | 75%+ |
| Devices | 80%+ |
| Protocols | 60%+ |
| Web | 50%+ |

## Test Dependencies

Required packages for testing:

```bash
# Ubuntu/Debian
sudo apt-get install libgtest-dev lcov

# The tests CMake will auto-fetch GTest if not found
```

## Troubleshooting

### Tests not found

Ensure you configured with `-DSMARTHUB_BUILD_TESTS=ON`:

```bash
cmake -B build -DSMARTHUB_BUILD_TESTS=ON
```

### Coverage report empty

Ensure you built with debug and coverage flags:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSMARTHUB_COVERAGE=ON
```

### Cross-compilation test failures

Tests are disabled for cross-compilation (`-DSMARTHUB_BUILD_TESTS=OFF`) since they cannot run on the host. Run tests natively first, then cross-compile for deployment.

## Design Decisions

### Why Google Test?

- Industry standard for C++ testing
- Excellent CMake integration
- Rich assertion library
- Test fixtures and parameterized tests
- Easy to auto-fetch via FetchContent

### Why Separate Test Library?

The `smarthub_lib` static library contains all testable code (excluding main.cpp). This:
- Avoids recompiling sources for each test
- Enables linking tests against the same code as the main binary
- Simplifies dependency management

### Test Isolation

Each test file is self-contained and can run independently. Integration tests use temporary files/databases that are cleaned up automatically.

## Mock Objects

Phase 3 introduced mock implementations for testing:

### MockDevice

Located in `tests/mocks/MockDevice.hpp`:

```cpp
#include "mocks/MockDevice.hpp"

MockDevice device("mock1", "Mock Device", DeviceType::Switch);

// Track state changes
device.setState("on", true);
EXPECT_EQ(device.setStateCalls, 1);
EXPECT_EQ(device.lastProperty, "on");

// MockDimmerDevice for brightness testing
MockDimmerDevice dimmer("dim1", "Mock Dimmer");

// MockSensorDevice for sensor testing
MockSensorDevice sensor("sensor1", "Mock Sensor");
sensor.setTemperature(22.5);
```

### MockProtocolHandler

Located in `tests/mocks/MockProtocolHandler.hpp`:

```cpp
#include "mocks/MockProtocolHandler.hpp"

EventBus eventBus;
MockProtocolHandler handler(eventBus, {});

// Lifecycle testing
EXPECT_TRUE(handler.initialize());
EXPECT_TRUE(handler.isConnected());

// Simulate device discovery
auto device = std::make_shared<MockDevice>();
handler.simulateDeviceDiscovered(device);

// Simulate state changes
handler.simulateStateChange("device1", "on", true);

// Verify commands
handler.sendCommand("addr", "set", {{"brightness", 50}});
EXPECT_EQ(handler.lastCommand, "set");
EXPECT_EQ(handler.commandCount, 1);

// Register mock protocol for factory tests
registerMockProtocol();
```

## Optional Dependency Testing

The Config tests include tests for the INI-style fallback parser, which is used when yaml-cpp is not available (e.g., during cross-compilation without yaml-cpp in the sysroot):

| Test | Description |
|------|-------------|
| ConfigIniTest.LoadIniFormat | INI-style config file loads correctly |
| ConfigIniTest.IniDatabasePath | Database path parsed from INI format |
| ConfigIniTest.IniWithQuotedValues | Quoted strings parsed correctly |
| ConfigIniTest.IniWithColonSeparator | YAML-style colon separator works |
| ConfigIniTest.IniEmptyFile | Empty config uses defaults |
| ConfigIniTest.IniWithWhitespace | Whitespace trimmed correctly |

This ensures the application works correctly when cross-compiled for ARM targets where yaml-cpp may not be available in the Buildroot sysroot.

## Zigbee Testing

The Zigbee tests (`protocols/test_zigbee.cpp`) provide comprehensive coverage of the Zigbee protocol stack without requiring hardware:

| Test Suite | Tests | Description |
|------------|-------|-------------|
| ZnpFrameTest | 9 | Frame building, serialization, parsing, FCS validation |
| ZnpTransportTest | 4 | Serial port abstraction, open/close, send |
| ZclConstantsTest | 4 | Data type sizes, cluster/attribute constants |
| ZigbeeDeviceDatabaseTest | 8 | JSON loading, lookup, device type parsing |
| ZigbeeCoordinatorTest | 3 | Initial state, device lookup |
| ZigbeeHandlerTest | 3 | Configuration, status, callbacks |
| ZclAttributeValueTest | 6 | Type conversions (bool, uint, int, string) |
| ZigbeeDeviceInfoTest | 2 | Default values, cluster storage |
| ZigbeeIntegrationTest | 2 | Frame roundtrip, database with quirks |
| ZigbeeProtocolTest | 1 | Handler creation |

### Mock Serial Port

The tests use a `MockSerialPort` class that implements `ISerialPort`:

```cpp
class MockSerialPort : public ISerialPort {
public:
    bool open(const std::string& port, int baud) override;
    ssize_t write(const uint8_t* data, size_t len) override;
    ssize_t read(uint8_t* buf, size_t max, int timeout) override;

    // Test helpers
    void queueReadData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> getWrittenData();
    void setOpenSuccess(bool success);
};
```

This enables testing the full protocol stack without a CC2652P coordinator:

```cpp
auto mockSerial = std::make_unique<MockSerialPort>();
mockSerial->queueReadData({0xFE, 0x00, 0x61, 0x01, 0x60});  // SRSP

auto transport = std::make_unique<ZnpTransport>(std::move(mockSerial), "/dev/ttyUSB0", 115200);
transport->open();

ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);
auto response = transport->request(frame);
EXPECT_TRUE(response.has_value());
```

### Running Zigbee Tests

```bash
cd app/build-test
cmake .. -DSMARTHUB_BUILD_TESTS=ON -DSMARTHUB_NATIVE_BUILD=ON
make test_zigbee
./tests/test_zigbee
```

## UI Testing

The UI tests (`ui/test_ui.cpp`) verify the UIManager DRM backend:

| Test | Description |
|------|-------------|
| Construction | UIManager can be constructed without crash |
| InitialStateNotRunning | isRunning() returns false before init |
| InitializeFailsWithInvalidDevice | Graceful failure with non-existent DRM device |
| InitializeFailsWithInvalidPath | Graceful failure with invalid path |
| DefaultDimensions | Default width/height are set correctly |
| ShutdownWithoutInit | shutdown() is safe without initialization |
| MultipleShutdownCalls | Multiple shutdown() calls are safe |
| UpdateWithoutInit | update() is safe without initialization |
| DestructorUninitialized | Destructor handles uninitialized state |
| DestructorAfterFailedInit | Destructor handles failed initialization |
| InitializeWithRealDRM | Hardware test (skipped if /dev/dri/card0 not available) |

### Running UI Tests

**On host (without LVGL/DRM):**
```bash
# Tests are skipped when LVGL not available
./build/tests/test_ui
# Output: [SKIPPED] UIManagerTest.LVGLNotAvailable
```

**On target hardware:**
```bash
# With /dev/dri/card0 accessible
./test_ui
```

### Conditional Compilation

The UI tests use conditional compilation:
- When `SMARTHUB_ENABLE_LVGL` is defined: Full test suite runs
- When LVGL not available: Single placeholder test that skips

This allows the same test file to work in both native and cross-compiled environments.

## M4 Firmware Testing

The M4 firmware (`m4-firmware/`) is a bare-metal Cortex-M4 application that communicates with the main Linux application via RPMsg.

### Host-Side Unit Tests (115 tests)

The M4 firmware includes comprehensive host-side unit tests that run on x86 with mocked hardware:

```bash
cd m4-firmware/tests
mkdir -p build && cd build
cmake ..
make
ctest --output-on-failure
```

| Test Suite | Tests | Description |
|------------|-------|-------------|
| MessageTypesTest | 25 | Message structure sizes, enum values, wire format |
| RpmsgProtocolTest | 20 | Message building, parsing, roundtrip, wire format |
| SHT31CalculationsTest | 28 | CRC-8 validation, temperature/humidity conversions |
| I2CTransactionsTest | 21 | Mock I2C operations, SHT31-specific transactions |
| SensorManagerTest | 21 | Polling logic, timing, sensor presence |

### What's Tested

- **Message Protocol**: Header/payload structure, serialization, command/response pairing
- **CRC-8 Calculation**: Sensirion CRC polynomial (0x31), validation
- **Temperature Conversion**: Raw to Celsius (-45°C to +130°C range)
- **Humidity Conversion**: Raw to %RH (0-100% range)
- **Fixed-point Encoding**: x100 scaling for RPMsg transmission
- **I2C Transactions**: Probe, read, write, register operations
- **Sensor Polling**: Interval timing, first-poll behavior, multiple sensors

### Build Verification

```bash
cd m4-firmware
mkdir -p build && cd build
cmake ..
make
```

Expected output:
- `smarthub-m4.elf` - ELF executable (~20KB)
- `smarthub-m4.bin` - Raw binary (~3KB)
- Build should show ARM hard-float ABI

### Verifying ELF Properties

```bash
arm-none-eabi-readelf -h build/smarthub-m4.elf
```

Expected:
- Machine: ARM
- Flags: hard-float ABI
- Entry point: 0x100000xx (MCUSRAM region)

### Hardware Testing

M4 firmware requires the STM32MP157F-DK2 board for full testing:

| Test | Method | Expected |
|------|--------|----------|
| Firmware load | `cat /sys/class/remoteproc/remoteproc0/state` | "running" |
| RPMsg channel | `ls /dev/ttyRPMSG*` | Device exists |
| Ping/pong | Send 0x01, read response | Receive 0x81 |
| Sensor data | Read RPMsg channel | 0x90 messages with sensor values |

### A7-Side RPMsg Tests

The A7 application includes RPMsg tests (`app/tests/rpmsg/test_rpmsg.cpp`) that verify:

- Message type encoding/decoding
- Ping/pong protocol
- Sensor data parsing
- GPIO command encoding

These tests use mock implementations and don't require hardware.

## Future Enhancements

- [x] Mock objects for external dependencies (MockDevice, MockProtocolHandler)
- [x] INI fallback parser tests for cross-compilation scenarios
- [x] UI/DRM backend tests with graceful skipping when hardware unavailable
- [x] M4 firmware compilation verification
- [ ] Hardware-in-the-loop tests on target device
- [ ] Performance benchmarks
- [ ] Fuzz testing for network protocols
- [ ] Memory leak detection with AddressSanitizer
- [ ] Mock MQTT broker for protocol testing
