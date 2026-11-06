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

#include "headless_display.hpp"
#include <lvgl.h>

#include <memory>
#include <stdexcept>
#include <thread>
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
    /** Navigation takes effect immediately, unless a test sets a duration itself. */
    void createScreenManager() {
        screenManager = std::make_unique<ScreenManager>(*uiManager);
        screenManager->setTransitionDuration(0);
    }

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
        createScreenManager();
    });
    EXPECT_NE(screenManager, nullptr);
}

// Test: Register screen
TEST_F(ScreenManagerTest, RegisterScreen) {
    createScreenManager();

    auto screen = std::make_unique<TestScreen>(*screenManager, "test");
    TestScreen* rawPtr = screen.get();

    screenManager->registerScreen("test", std::move(screen));

    EXPECT_TRUE(screenManager->hasScreen("test"));
    EXPECT_EQ(screenManager->getScreen("test"), rawPtr);
    EXPECT_TRUE(rawPtr->createCalled);  // onCreate should be called
}

// Test: Unregister screen
TEST_F(ScreenManagerTest, UnregisterScreen) {
    createScreenManager();

    auto screen = std::make_unique<TestScreen>(*screenManager, "test");
    screenManager->registerScreen("test", std::move(screen));

    EXPECT_TRUE(screenManager->hasScreen("test"));
    EXPECT_TRUE(screenManager->unregisterScreen("test"));
    EXPECT_FALSE(screenManager->hasScreen("test"));
    EXPECT_EQ(screenManager->getScreen("test"), nullptr);
}

// Test: Unregister non-existent screen returns false
TEST_F(ScreenManagerTest, UnregisterNonExistent) {
    createScreenManager();
    EXPECT_FALSE(screenManager->unregisterScreen("nonexistent"));
}

// Test: Navigate to screen
TEST_F(ScreenManagerTest, ShowScreen) {
    createScreenManager();

    auto screen = std::make_unique<TestScreen>(*screenManager, "home");
    TestScreen* rawPtr = screen.get();
    screenManager->registerScreen("home", std::move(screen));

    EXPECT_TRUE(screenManager->showScreen("home"));
    EXPECT_EQ(screenManager->currentScreen(), rawPtr);
    EXPECT_EQ(rawPtr->showCount, 1);
}

// Test: Navigate to non-existent screen fails
TEST_F(ScreenManagerTest, ShowNonExistent) {
    createScreenManager();
    EXPECT_FALSE(screenManager->showScreen("nonexistent"));
    EXPECT_EQ(screenManager->currentScreen(), nullptr);
}

// Test: Navigation builds history stack
TEST_F(ScreenManagerTest, NavigationHistory) {
    createScreenManager();

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
    createScreenManager();

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
    createScreenManager();

    auto home = std::make_unique<TestScreen>(*screenManager, "home");
    screenManager->registerScreen("home", std::move(home));
    screenManager->showScreen("home");

    EXPECT_FALSE(screenManager->goBack());  // No history
}

// Test: Go home clears history
TEST_F(ScreenManagerTest, GoHome) {
    createScreenManager();

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
    createScreenManager();

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
    createScreenManager();

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
    createScreenManager();

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

// Test: Removing the screen on display leaves LVGL able to load another
TEST_F(ScreenManagerTest, UnregisteringTheActiveScreen) {
    createScreenManager();
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));

    ASSERT_TRUE(screenManager->showScreen("home"));
    ASSERT_TRUE(screenManager->unregisterScreen("home"));

    EXPECT_NE(lv_scr_act(), nullptr);
    EXPECT_TRUE(screenManager->showScreen("settings"));
    test::render();
}

// Test: Removing a screen while it's being loaded doesn't leave LVGL holding it
TEST_F(ScreenManagerTest, UnregisteringAScreenBeingLoaded) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    screenManager->setTransitionDuration(300);
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));

    ASSERT_TRUE(screenManager->showScreen("home"));
    ASSERT_TRUE(test::transitionPending());
    ASSERT_TRUE(screenManager->unregisterScreen("home"));

    EXPECT_FALSE(test::transitionPending());
    EXPECT_FALSE(screenManager->isTransitioning());
    test::render();

    EXPECT_TRUE(screenManager->showScreen("settings"));
    test::finishTransitions();
    test::render();
}

// Test: Removing the outgoing screen during a transition finishes that transition
TEST_F(ScreenManagerTest, UnregisteringTheOutgoingScreenDuringATransition) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    screenManager->setTransitionDuration(0);
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));
    ASSERT_TRUE(screenManager->showScreen("home"));
    lv_obj_t* settingsContainer = screenManager->getScreen("settings")->container();

    screenManager->setTransitionDuration(300);
    ASSERT_TRUE(screenManager->showScreen("settings"));
    ASSERT_TRUE(test::transitionPending());
    ASSERT_TRUE(screenManager->unregisterScreen("home"));

    EXPECT_FALSE(test::transitionPending());
    EXPECT_EQ(lv_scr_act(), settingsContainer);
    test::render();
}

// Test: Same, once the animation is running and LVGL has moved on to the new screen
TEST_F(ScreenManagerTest, UnregisteringTheOutgoingScreenOnceTheAnimationIsRunning) {
    createScreenManager();
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));
    ASSERT_TRUE(screenManager->showScreen("home"));
    lv_obj_t* settingsContainer = screenManager->getScreen("settings")->container();

    screenManager->setTransitionDuration(300);
    ASSERT_TRUE(screenManager->showScreen("settings"));
    for (int frame = 0; frame < 5; ++frame) {
        lv_tick_inc(16);
        lv_timer_handler();
    }
    lv_disp_t* display = lv_disp_get_default();
    ASSERT_NE(display->prev_scr, nullptr);
    ASSERT_EQ(lv_scr_act(), settingsContainer);

    ASSERT_TRUE(screenManager->unregisterScreen("home"));

    EXPECT_FALSE(test::transitionPending());
    EXPECT_EQ(display->prev_scr, nullptr);
    EXPECT_EQ(lv_scr_act(), settingsContainer);
    test::render();
    EXPECT_EQ(lv_obj_get_x(settingsContainer), 0);
}

// Test: Removing a screen that isn't involved leaves a transition running
TEST_F(ScreenManagerTest, UnregisteringAnUnrelatedScreenLeavesTheTransitionAlone) {
    createScreenManager();
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));
    screenManager->registerScreen("about", std::make_unique<TestScreen>(*screenManager, "about"));
    ASSERT_TRUE(screenManager->showScreen("home"));
    lv_obj_t* settingsContainer = screenManager->getScreen("settings")->container();

    screenManager->setTransitionDuration(300);
    ASSERT_TRUE(screenManager->showScreen("settings"));
    ASSERT_TRUE(screenManager->unregisterScreen("about"));

    EXPECT_TRUE(test::transitionPending());
    EXPECT_TRUE(screenManager->isTransitioning());
    test::finishTransitions();
    EXPECT_EQ(lv_scr_act(), settingsContainer);
}

// Test: Removing the screen on display doesn't accumulate replacement screens
TEST_F(ScreenManagerTest, UnregisteringTheActiveScreenLeavesNothingBehind) {
    createScreenManager();
    lv_disp_t* display = lv_disp_get_default();
    const uint32_t screensBefore = display->screen_cnt;

    for (int i = 0; i < 5; ++i) {
        screenManager->registerScreen("temp", std::make_unique<TestScreen>(*screenManager, "temp"));
        ASSERT_TRUE(screenManager->showScreen("temp"));
        ASSERT_TRUE(screenManager->unregisterScreen("temp"));
    }

    EXPECT_EQ(display->screen_cnt, screensBefore);
    EXPECT_NE(lv_scr_act(), nullptr);
}

// Test: A screen load left pending doesn't crash the reset between tests
TEST_F(ScreenManagerTest, LeavesAScreenLoadPendingForTheReset) {
    lv_scr_load_anim(lv_obj_create(nullptr), LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);

    EXPECT_TRUE(test::transitionPending());
}

// Test: Navigation is ignored until the transition finishes
TEST_F(ScreenManagerTest, NavigationIsBlockedDuringTransition) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    screenManager->setTransitionDuration(300);

    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));

    ASSERT_TRUE(screenManager->showScreen("home"));
    EXPECT_TRUE(screenManager->isTransitioning());
    EXPECT_FALSE(screenManager->showScreen("settings"));
    EXPECT_EQ(screenManager->currentScreen()->name(), "home");

    // Run the app's main loop until the transition is over
    for (int i = 0; i < 100 && screenManager->isTransitioning(); ++i) {
        lv_tick_inc(16);
        screenManager->update(16);
        lv_timer_handler();
    }
    EXPECT_FALSE(screenManager->isTransitioning());

    EXPECT_TRUE(screenManager->showScreen("settings"));
    EXPECT_EQ(screenManager->currentScreen()->name(), "settings");
}

// Issue #27: runs the same steps as UIManager's main loop, at 60 Hz. LVGL crashes
// on a null pointer if a screen load starts while another is pending, so
// ScreenManager must not report a transition as over before LVGL has finished.
TEST_F(ScreenManagerTest, TransitionIsNotReportedOverBeforeLvglFinishes) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    screenManager->setTransitionDuration(300);

    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    ASSERT_TRUE(screenManager->showScreen("home"));

    int frames = 0;
    for (; frames < 100 && screenManager->isTransitioning(); ++frames) {
        lv_tick_inc(16);
        screenManager->update(16);
        lv_timer_handler();
    }

    // Held until the animation's 300 ms had passed, at 16 ms a frame, and then released
    EXPECT_GE(frames * 16, 300);
    EXPECT_FALSE(screenManager->isTransitioning());
    EXPECT_FALSE(test::transitionPending());
}

TEST_F(ScreenManagerTest, EveryKindOfNavigationWaitsForATransition) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    screenManager->setTransitionDuration(0);
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));
    screenManager->registerScreen("about", std::make_unique<TestScreen>(*screenManager, "about"));
    screenManager->setHomeScreen("home");

    ASSERT_TRUE(screenManager->showScreen("home"));
    ASSERT_TRUE(screenManager->showScreen("settings"));
    screenManager->setTransitionDuration(300);
    ASSERT_TRUE(screenManager->showScreen("about"));
    ASSERT_TRUE(screenManager->isTransitioning());

    // Tried during the transition: none of these may crash, or lose the way back
    EXPECT_FALSE(screenManager->showScreen("home", TransitionType::None));
    EXPECT_FALSE(screenManager->goBack());
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen()->name(), "about");

    test::finishTransitions();
    screenManager->setTransitionDuration(0);
    EXPECT_TRUE(screenManager->goBack());
    EXPECT_EQ(screenManager->currentScreen()->name(), "settings");
}

TEST_F(ScreenManagerTest, FailedGoBackKeepsTheHistory) {
    createScreenManager();
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));
    screenManager->registerScreen("about", std::make_unique<TestScreen>(*screenManager, "about"));
    ASSERT_TRUE(screenManager->showScreen("home"));
    ASSERT_TRUE(screenManager->showScreen("settings"));
    ASSERT_TRUE(screenManager->showScreen("about"));

    screenManager->setTransitionDuration(300);
    ASSERT_TRUE(screenManager->goBack());
    EXPECT_EQ(screenManager->currentScreen()->name(), "settings");
    EXPECT_FALSE(screenManager->goBack());
    test::finishTransitions();

    EXPECT_TRUE(screenManager->goBack());
    EXPECT_EQ(screenManager->currentScreen()->name(), "home");
}

// Issue #27: navigating on each frame of a transition, including the one where LVGL finishes it.
// Before the fix, one of these crashed inside LVGL.
TEST_F(ScreenManagerTest, NavigatingOnEveryFrameOfATransitionIsSafe) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);
    screenManager->setTransitionDuration(300);
    screenManager->registerScreen("home", std::make_unique<TestScreen>(*screenManager, "home"));
    screenManager->registerScreen("settings", std::make_unique<TestScreen>(*screenManager, "settings"));

    ASSERT_TRUE(screenManager->showScreen("home"));

    bool navigated = false;
    for (int frame = 0; frame < 100 && !navigated; ++frame) {
        lv_tick_inc(16);
        screenManager->update(16);
        navigated = screenManager->showScreen("settings");
        lv_timer_handler();
    }

    EXPECT_TRUE(navigated);
    test::finishTransitions();
    EXPECT_EQ(screenManager->currentScreen()->name(), "settings");
}

// Test: Same screen navigation is no-op
TEST_F(ScreenManagerTest, SameScreenNavigation) {
    createScreenManager();

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

#ifdef SMARTHUB_ENABLE_LVGL

// ---------- Tasks posted to the UI thread

namespace {

// Lets a test ask for a task through the screen itself
class PostingScreen : public TestScreen {
public:
    using TestScreen::TestScreen;
    using TestScreen::UiPoster;
    using TestScreen::uiPoster;
};

}  // namespace

TEST_F(ScreenManagerTest, PostedTaskRunsOnTheNextUpdate) {
    createScreenManager();

    std::thread::id ranOn;
    std::thread poster([&] {
        screenManager->postToUi([&] { ranOn = std::this_thread::get_id(); });
    });
    poster.join();
    EXPECT_EQ(ranOn, std::thread::id{});

    screenManager->update(16);
    EXPECT_EQ(ranOn, std::this_thread::get_id());
}

TEST_F(ScreenManagerTest, PostedTaskRunsOnlyOnce) {
    createScreenManager();
    int runs = 0;
    screenManager->postToUi([&] { ++runs; });

    screenManager->update(16);
    screenManager->update(16);
    EXPECT_EQ(runs, 1);
}

TEST_F(ScreenManagerTest, TaskCanPostAnotherForTheNextUpdate) {
    createScreenManager();
    int runs = 0;
    screenManager->postToUi([&] {
        ++runs;
        screenManager->postToUi([&] { ++runs; });
    });

    screenManager->update(16);
    EXPECT_EQ(runs, 1);
    screenManager->update(16);
    EXPECT_EQ(runs, 2);
}

TEST_F(ScreenManagerTest, FailingTaskDoesNotStopTheOthers) {
    createScreenManager();
    bool ran = false;
    screenManager->postToUi([] { throw std::runtime_error("failed"); });
    screenManager->postToUi([] { throw 1; });
    screenManager->postToUi([&] { ran = true; });

    EXPECT_NO_THROW(screenManager->update(16));
    EXPECT_TRUE(ran);
}

TEST_F(ScreenManagerTest, ScreensPostedTaskSkipsIfTheScreenIsDestroyed) {
    createScreenManager();
    bool ran = false;
    PostingScreen::UiPoster poster;
    {
        PostingScreen screen(*screenManager, "posting");
        poster = screen.uiPoster();
    }

    // Safe even after the screen has gone
    poster([&] { ran = true; });

    screenManager->update(16);
    EXPECT_FALSE(ran);
}

TEST_F(ScreenManagerTest, ScreensPostedTaskRunsWhileTheScreenExists) {
    createScreenManager();
    bool ran = false;
    PostingScreen screen(*screenManager, "posting");
    screen.uiPoster()([&] { ran = true; });

    screenManager->update(16);
    EXPECT_TRUE(ran);
}

#endif // SMARTHUB_ENABLE_LVGL
