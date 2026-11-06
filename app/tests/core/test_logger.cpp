/**
 * Logger Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/core/Logger.hpp>
#include <cinttypes>
#include <cstdint>
#include <string>

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset logger to default state
        smarthub::Logger::init(smarthub::Logger::Level::Info, "");
    }
};

TEST_F(LoggerTest, SingletonInstance) {
    auto& logger1 = smarthub::Logger::instance();
    auto& logger2 = smarthub::Logger::instance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, InitSetsMinLevel) {
    smarthub::Logger::init(smarthub::Logger::Level::Warning, "");
    // Should not crash when logging at various levels
    LOG_DEBUG("This debug message should be filtered");
    LOG_INFO("This info message should be filtered");
    LOG_WARN("This warning should appear");
    LOG_ERROR("This error should appear");
}

TEST_F(LoggerTest, InitWithDebugLevel) {
    smarthub::Logger::init(smarthub::Logger::Level::Debug, "");
    // All levels should work at debug
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARN("Warning message");
    LOG_ERROR("Error message");
}

TEST_F(LoggerTest, FormatStrings) {
    // Just verify format string handling doesn't crash
    smarthub::Logger::init(smarthub::Logger::Level::Debug, "");
    LOG_INFO("Integer: %d", 42);
    LOG_INFO("String: %s", "test");
    LOG_INFO("Float: %.2f", 3.14);
    LOG_INFO("Multiple: %d, %s, %.1f", 1, "two", 3.0);
}

TEST_F(LoggerTest, RendersFormatArgumentsIntoTheLog) {
    // Every call site was converted from {} placeholders, which printed literally and dropped the
    // values. This checks the values reach the log, including a 64-bit address on the 32-bit board.
    const uint64_t ieee = 0x00124B0012345678ULL;
    testing::internal::CaptureStderr();
    LOG_ERROR("Device %016" PRIX64 " on port %s, code %d", ieee, "/dev/ttyUSB0", 7);
    const std::string output = testing::internal::GetCapturedStderr();

    EXPECT_NE(output.find("Device 00124B0012345678 on port /dev/ttyUSB0, code 7"), std::string::npos) << output;
}

TEST_F(LoggerTest, EmptyMessage) {
    smarthub::Logger::init(smarthub::Logger::Level::Debug, "");
    LOG_INFO("%s", "");  // Should not crash
}

TEST_F(LoggerTest, LongMessage) {
    smarthub::Logger::init(smarthub::Logger::Level::Debug, "");
    std::string longMsg(1000, 'x');
    LOG_INFO("%s", longMsg.c_str());  // Should handle long messages
}
