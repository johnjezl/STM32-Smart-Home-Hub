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
├── CMakeLists.txt          # Test build configuration
├── main.cpp                # Main test runner entry point
├── core/
│   ├── test_logger.cpp     # Logger unit tests
│   ├── test_config.cpp     # Config unit tests
│   └── test_eventbus.cpp   # EventBus unit tests
├── database/
│   └── test_database.cpp   # Database unit tests
├── devices/
│   ├── test_device.cpp     # Device unit tests
│   └── test_device_manager.cpp  # DeviceManager unit tests
├── protocols/              # Protocol tests (future)
├── rpmsg/                  # RPMsg tests (future)
├── web/                    # Web server tests (future)
├── mocks/                  # Mock objects (future)
└── integration/
    └── test_integration.cpp  # Integration tests
```

## Test Categories

### Unit Tests

Unit tests verify individual components in isolation:

| Test Suite | File | Description |
|------------|------|-------------|
| Logger | `core/test_logger.cpp` | Logging levels, file output, formatting |
| Config | `core/test_config.cpp` | YAML loading, default values, getters |
| EventBus | `core/test_eventbus.cpp` | Pub/sub, async events, unsubscribe |
| Database | `database/test_database.cpp` | SQLite operations, schema, transactions |
| Device | `devices/test_device.cpp` | Device properties, state, events |
| DeviceManager | `devices/test_device_manager.cpp` | Add/remove, queries, events |

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

## Future Enhancements

- [ ] Mock objects for external dependencies (MQTT, filesystem)
- [ ] Hardware-in-the-loop tests on target device
- [ ] Performance benchmarks
- [ ] Fuzz testing for network protocols
- [ ] Memory leak detection with AddressSanitizer
