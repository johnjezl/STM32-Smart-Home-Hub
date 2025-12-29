/**
 * UIManager Unit Tests
 *
 * Tests the LVGL-based UI manager with DRM backend.
 * Note: These tests run without actual DRM hardware, testing
 * graceful failure handling and API behavior.
 */

#include <gtest/gtest.h>

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/UIManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/database/Database.hpp>

#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

class UIManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath = "/tmp/ui_test_" + std::to_string(getpid()) + ".db";

        // Clean up any existing test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }

        // Initialize required components
        eventBus = std::make_unique<smarthub::EventBus>();
        database = std::make_unique<smarthub::Database>(testDbPath);
        database->initialize();
        deviceManager = std::make_unique<smarthub::DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
    }

    void TearDown() override {
        uiManager.reset();
        deviceManager.reset();
        database.reset();
        eventBus.reset();

        // Clean up test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    std::string testDbPath;
    std::unique_ptr<smarthub::EventBus> eventBus;
    std::unique_ptr<smarthub::Database> database;
    std::unique_ptr<smarthub::DeviceManager> deviceManager;
    std::unique_ptr<smarthub::UIManager> uiManager;
};

// Test: UIManager can be constructed
TEST_F(UIManagerTest, Construction) {
    ASSERT_NO_THROW({
        uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);
    });
    EXPECT_NE(uiManager, nullptr);
}

// Test: UIManager reports not running before initialization
TEST_F(UIManagerTest, InitialStateNotRunning) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);
    EXPECT_FALSE(uiManager->isRunning());
}

// Test: Initialize fails gracefully with non-existent DRM device
TEST_F(UIManagerTest, InitializeFailsWithInvalidDevice) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Try to initialize with a non-existent device
    bool result = uiManager->initialize("/dev/dri/nonexistent_card", "/dev/input/event99");

    EXPECT_FALSE(result);
    EXPECT_FALSE(uiManager->isRunning());
}

// Test: Initialize fails gracefully with invalid path
TEST_F(UIManagerTest, InitializeFailsWithInvalidPath) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Try to initialize with an invalid path
    bool result = uiManager->initialize("/invalid/path/to/device", "/invalid/touch");

    EXPECT_FALSE(result);
    EXPECT_FALSE(uiManager->isRunning());
}

// Test: Default dimensions before initialization
TEST_F(UIManagerTest, DefaultDimensions) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Default dimensions should be set (480x800 as per header)
    EXPECT_EQ(uiManager->getWidth(), 480);
    EXPECT_EQ(uiManager->getHeight(), 800);
}

// Test: Shutdown is safe to call without initialization
TEST_F(UIManagerTest, ShutdownWithoutInit) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Shutdown should be safe even if not initialized
    ASSERT_NO_THROW({
        uiManager->shutdown();
    });
    EXPECT_FALSE(uiManager->isRunning());
}

// Test: Multiple shutdown calls are safe
TEST_F(UIManagerTest, MultipleShutdownCalls) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Multiple shutdown calls should be safe
    ASSERT_NO_THROW({
        uiManager->shutdown();
        uiManager->shutdown();
        uiManager->shutdown();
    });
}

// Test: Update is safe to call without initialization
TEST_F(UIManagerTest, UpdateWithoutInit) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Update should be safe even if not initialized
    ASSERT_NO_THROW({
        uiManager->update();
    });
}

// Test: Destructor handles uninitialized state
TEST_F(UIManagerTest, DestructorUninitialized) {
    {
        auto tempUi = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);
        // Let it go out of scope without initialization
    }
    // If we get here without crash, the test passes
    SUCCEED();
}

// Test: Destructor handles failed initialization
TEST_F(UIManagerTest, DestructorAfterFailedInit) {
    {
        auto tempUi = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);
        tempUi->initialize("/dev/dri/nonexistent", "/dev/input/event99");
        // Let it go out of scope after failed init
    }
    // If we get here without crash, the test passes
    SUCCEED();
}

#ifdef __linux__
// Test: Initialize with /dev/dri/card0 if it exists (hardware-dependent)
TEST_F(UIManagerTest, InitializeWithRealDRM) {
    uiManager = std::make_unique<smarthub::UIManager>(*eventBus, *deviceManager);

    // Check if DRM device exists
    if (access("/dev/dri/card0", R_OK | W_OK) != 0) {
        GTEST_SKIP() << "DRM device /dev/dri/card0 not accessible, skipping hardware test";
    }

    // Try to initialize with real device
    // This may still fail if no display is connected
    bool result = uiManager->initialize("/dev/dri/card0", "/dev/input/event0");

    if (result) {
        EXPECT_TRUE(uiManager->isRunning());
        EXPECT_GT(uiManager->getWidth(), 0);
        EXPECT_GT(uiManager->getHeight(), 0);

        // Test update
        ASSERT_NO_THROW({
            uiManager->update();
        });

        // Test shutdown
        ASSERT_NO_THROW({
            uiManager->shutdown();
        });
        EXPECT_FALSE(uiManager->isRunning());
    } else {
        // Failed init is acceptable if no display connected
        EXPECT_FALSE(uiManager->isRunning());
    }
}
#endif

#else // !SMARTHUB_ENABLE_LVGL

// Placeholder test when LVGL is not available
TEST(UIManagerTest, LVGLNotAvailable) {
    GTEST_SKIP() << "LVGL not available, UI tests skipped";
}

#endif // SMARTHUB_ENABLE_LVGL
