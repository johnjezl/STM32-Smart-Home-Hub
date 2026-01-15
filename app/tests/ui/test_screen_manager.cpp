/**
 * ScreenManager Unit Tests
 *
 * Tests screen navigation, lifecycle management, and history.
 * Note: LVGL-specific functionality tested separately on hardware.
 */

#include <gtest/gtest.h>

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/Screen.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/database/Database.hpp>

#include <memory>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
using namespace smarthub;
using namespace smarthub::ui;

// Test screen that tracks lifecycle calls
class TestScreen : public Screen {
public:
    TestScreen(ScreenManager& mgr, const std::string& name)
        : Screen(mgr, name) {}

    void onCreate() override { createCalled = true; }
    void onShow() override { showCount++; }
    void onHide() override { hideCount++; }
    void onDestroy() override { destroyCalled = true; }
    void onUpdate(uint32_t deltaMs) override {
        updateCount++;
        lastDeltaMs = deltaMs;
    }

    bool createCalled = false;
    int showCount = 0;
    int hideCount = 0;
    bool destroyCalled = false;
    int updateCount = 0;
    uint32_t lastDeltaMs = 0;
};

class ScreenManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath = "/tmp/screen_mgr_test_" + std::to_string(getpid()) + ".db";

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }

        eventBus = std::make_unique<EventBus>();
        database = std::make_unique<Database>(testDbPath);
        database->initialize();
        deviceManager = std::make_unique<DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
        uiManager = std::make_unique<UIManager>(*eventBus, *deviceManager, *database);

        // Note: We don't initialize UIManager (no display in CI)
        // but ScreenManager should still work for navigation logic
    }

    void TearDown() override {
        screenManager.reset();
        uiManager.reset();
        deviceManager.reset();
        database.reset();
        eventBus.reset();

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    std::string testDbPath;
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<ScreenManager> screenManager;
};

// Test: ScreenManager can be constructed
TEST_F(ScreenManagerTest, Construction) {
    ASSERT_NO_THROW({
        screenManager = std::make_unique<ScreenManager>(*uiManager);
    });
    EXPECT_NE(screenManager, nullptr);
}

// Test: Register screen
TEST_F(ScreenManagerTest, RegisterScreen) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto screen = std::make_unique<TestScreen>(*screenManager, "test");
    TestScreen* rawPtr = screen.get();

    screenManager->registerScreen("test", std::move(screen));

    EXPECT_TRUE(screenManager->hasScreen("test"));
    EXPECT_EQ(screenManager->getScreen("test"), rawPtr);
    EXPECT_TRUE(rawPtr->createCalled);  // onCreate should be called
}

// Test: Unregister screen
TEST_F(ScreenManagerTest, UnregisterScreen) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto screen = std::make_unique<TestScreen>(*screenManager, "test");
    screenManager->registerScreen("test", std::move(screen));

    EXPECT_TRUE(screenManager->hasScreen("test"));
    EXPECT_TRUE(screenManager->unregisterScreen("test"));
    EXPECT_FALSE(screenManager->hasScreen("test"));
    EXPECT_EQ(screenManager->getScreen("test"), nullptr);
}

// Test: Unregister non-existent screen returns false
TEST_F(ScreenManagerTest, UnregisterNonExistent) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    EXPECT_FALSE(screenManager->unregisterScreen("nonexistent"));
}

// Test: Navigate to screen
TEST_F(ScreenManagerTest, ShowScreen) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto screen = std::make_unique<TestScreen>(*screenManager, "home");
    TestScreen* rawPtr = screen.get();
    screenManager->registerScreen("home", std::move(screen));

    EXPECT_TRUE(screenManager->showScreen("home"));
    EXPECT_EQ(screenManager->currentScreen(), rawPtr);
    EXPECT_EQ(rawPtr->showCount, 1);
}

// Test: Navigate to non-existent screen fails
TEST_F(ScreenManagerTest, ShowNonExistent) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    EXPECT_FALSE(screenManager->showScreen("nonexistent"));
    EXPECT_EQ(screenManager->currentScreen(), nullptr);
}

// Test: Navigation builds history stack
TEST_F(ScreenManagerTest, NavigationHistory) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto home = std::make_unique<TestScreen>(*screenManager, "home");
    auto settings = std::make_unique<TestScreen>(*screenManager, "settings");
    auto about = std::make_unique<TestScreen>(*screenManager, "about");

    screenManager->registerScreen("home", std::move(home));
    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("about", std::move(about));

    screenManager->showScreen("home");
    EXPECT_EQ(screenManager->stackDepth(), 0);

    screenManager->showScreen("settings");
    EXPECT_EQ(screenManager->stackDepth(), 1);

    screenManager->showScreen("about");
    EXPECT_EQ(screenManager->stackDepth(), 2);
}

// Test: Go back navigates to previous screen
TEST_F(ScreenManagerTest, GoBack) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto home = std::make_unique<TestScreen>(*screenManager, "home");
    auto settings = std::make_unique<TestScreen>(*screenManager, "settings");
    TestScreen* homePtr = home.get();

    screenManager->registerScreen("home", std::move(home));
    screenManager->registerScreen("settings", std::move(settings));

    screenManager->showScreen("home");
    screenManager->showScreen("settings");

    EXPECT_TRUE(screenManager->goBack());
    EXPECT_EQ(screenManager->currentScreen(), homePtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

// Test: Go back with empty history
TEST_F(ScreenManagerTest, GoBackEmptyHistory) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto home = std::make_unique<TestScreen>(*screenManager, "home");
    screenManager->registerScreen("home", std::move(home));
    screenManager->showScreen("home");

    EXPECT_FALSE(screenManager->goBack());  // No history
}

// Test: Go home clears history
TEST_F(ScreenManagerTest, GoHome) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto home = std::make_unique<TestScreen>(*screenManager, "home");
    auto settings = std::make_unique<TestScreen>(*screenManager, "settings");
    auto about = std::make_unique<TestScreen>(*screenManager, "about");
    TestScreen* homePtr = home.get();

    screenManager->registerScreen("home", std::move(home));
    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("about", std::move(about));

    screenManager->setHomeScreen("home");
    screenManager->showScreen("home");
    screenManager->showScreen("settings");
    screenManager->showScreen("about");

    EXPECT_EQ(screenManager->stackDepth(), 2);

    screenManager->goHome();

    EXPECT_EQ(screenManager->currentScreen(), homePtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

// Test: Screen lifecycle - show/hide
TEST_F(ScreenManagerTest, ScreenLifecycleShowHide) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto screen1 = std::make_unique<TestScreen>(*screenManager, "screen1");
    auto screen2 = std::make_unique<TestScreen>(*screenManager, "screen2");
    TestScreen* ptr1 = screen1.get();
    TestScreen* ptr2 = screen2.get();

    screenManager->registerScreen("screen1", std::move(screen1));
    screenManager->registerScreen("screen2", std::move(screen2));

    screenManager->showScreen("screen1");
    EXPECT_EQ(ptr1->showCount, 1);
    EXPECT_EQ(ptr1->hideCount, 0);

    screenManager->showScreen("screen2");
    EXPECT_EQ(ptr1->showCount, 1);
    EXPECT_EQ(ptr1->hideCount, 1);  // screen1 hidden
    EXPECT_EQ(ptr2->showCount, 1);
    EXPECT_EQ(ptr2->hideCount, 0);
}

// Test: Update calls onUpdate on current screen
TEST_F(ScreenManagerTest, Update) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto screen = std::make_unique<TestScreen>(*screenManager, "test");
    TestScreen* rawPtr = screen.get();
    screenManager->registerScreen("test", std::move(screen));
    screenManager->showScreen("test");

    screenManager->update(16);
    EXPECT_EQ(rawPtr->updateCount, 1);
    EXPECT_EQ(rawPtr->lastDeltaMs, 16u);

    screenManager->update(33);
    EXPECT_EQ(rawPtr->updateCount, 2);
    EXPECT_EQ(rawPtr->lastDeltaMs, 33u);
}

// Test: Clear history
TEST_F(ScreenManagerTest, ClearHistory) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto home = std::make_unique<TestScreen>(*screenManager, "home");
    auto settings = std::make_unique<TestScreen>(*screenManager, "settings");

    screenManager->registerScreen("home", std::move(home));
    screenManager->registerScreen("settings", std::move(settings));

    screenManager->showScreen("home");
    screenManager->showScreen("settings");

    EXPECT_EQ(screenManager->stackDepth(), 1);

    screenManager->clearHistory();

    EXPECT_EQ(screenManager->stackDepth(), 0);
    // Still on settings screen
    EXPECT_EQ(screenManager->currentScreen()->name(), "settings");
}

// Test: Transition duration setting
TEST_F(ScreenManagerTest, TransitionDuration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    EXPECT_EQ(screenManager->transitionDuration(), 300);  // Default

    screenManager->setTransitionDuration(500);
    EXPECT_EQ(screenManager->transitionDuration(), 500);
}

// Test: Same screen navigation is no-op
TEST_F(ScreenManagerTest, SameScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto screen = std::make_unique<TestScreen>(*screenManager, "test");
    TestScreen* rawPtr = screen.get();
    screenManager->registerScreen("test", std::move(screen));

    screenManager->showScreen("test");
    EXPECT_EQ(rawPtr->showCount, 1);

    screenManager->showScreen("test");  // Navigate to same screen
    EXPECT_EQ(rawPtr->showCount, 1);    // Should not increment
    EXPECT_EQ(screenManager->stackDepth(), 0);  // Should not push to stack
}

#else // !SMARTHUB_ENABLE_LVGL

// Placeholder test when LVGL is not available
TEST(ScreenManagerTest, LVGLNotAvailable) {
    GTEST_SKIP() << "LVGL not available, ScreenManager tests skipped";
}

#endif // SMARTHUB_ENABLE_LVGL
