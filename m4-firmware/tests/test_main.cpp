/**
 * M4 Firmware Unit Tests - Main Entry Point
 *
 * These tests run on the host with mocked hardware to verify
 * business logic, message parsing, and calculations.
 */

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
