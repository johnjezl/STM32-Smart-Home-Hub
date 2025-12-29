/**
 * SmartHub Test Suite Main Entry Point
 *
 * Runs all registered Google Test tests.
 */

#include <gtest/gtest.h>
#include <smarthub/core/Logger.hpp>

int main(int argc, char** argv)
{
    // Initialize logger with minimal output for tests
    smarthub::Logger::init(smarthub::Logger::Level::Warning, "");

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Run all tests
    return RUN_ALL_TESTS();
}
