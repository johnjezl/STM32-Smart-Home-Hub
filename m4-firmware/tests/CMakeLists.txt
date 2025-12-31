# M4 Firmware Host-Side Unit Tests
#
# These tests run on the host (x86) with mocked hardware.
# They test business logic, message parsing, calculations, etc.

cmake_minimum_required(VERSION 3.16)
project(m4-firmware-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Define that we're in test/mock mode
add_definitions(-DM4_TESTING)
add_definitions(-DM4_HOST_BUILD)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Mock/testable sources (logic only, no hardware access)
set(TESTABLE_SOURCES
    # We'll create testable versions that use abstractions
)

# Test sources
set(TEST_SOURCES
    test_main.cpp
    test_rpmsg_protocol.cpp
    test_sht31_calculations.cpp
    test_i2c_transactions.cpp
    test_sensor_manager.cpp
    test_message_types.cpp
)

# Create test executable
add_executable(m4_tests
    ${TEST_SOURCES}
)

target_link_libraries(m4_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
enable_testing()
add_test(NAME M4FirmwareTests COMMAND m4_tests)

# Individual test targets for convenience
add_executable(test_rpmsg_protocol test_rpmsg_protocol.cpp)
target_link_libraries(test_rpmsg_protocol PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME RpmsgProtocol COMMAND test_rpmsg_protocol)

add_executable(test_sht31 test_sht31_calculations.cpp)
target_link_libraries(test_sht31 PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME SHT31Calculations COMMAND test_sht31)

add_executable(test_i2c test_i2c_transactions.cpp)
target_link_libraries(test_i2c PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME I2CTransactions COMMAND test_i2c)

add_executable(test_sensor_manager test_sensor_manager.cpp)
target_link_libraries(test_sensor_manager PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME SensorManager COMMAND test_sensor_manager)

add_executable(test_message_types test_message_types.cpp)
target_link_libraries(test_message_types PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME MessageTypes COMMAND test_message_types)
