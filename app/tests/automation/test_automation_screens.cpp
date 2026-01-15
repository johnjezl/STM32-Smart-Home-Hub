/**
 * Automation Screen Unit Tests
 *
 * Tests for AutomationListScreen, AddAutomationScreen, and EditAutomationScreen.
 * LVGL-specific rendering tested on hardware; these tests focus on
 * screen registration, navigation, and data handling.
 */

#include <gtest/gtest.h>

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/screens/AutomationListScreen.hpp>
#include <smarthub/ui/screens/AddAutomationScreen.hpp>
#include <smarthub/ui/screens/EditAutomationScreen.hpp>
#include <smarthub/ui/screens/DashboardScreen.hpp>
#include <smarthub/ui/screens/SettingsScreen.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/ThemeManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>

#include <memory>
#include <filesystem>

namespace fs = std::filesystem;
using namespace smarthub;
using namespace smarthub::ui;

// ============================================================================
// Test Fixture
// ============================================================================

class AutomationScreenTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath = "/tmp/auto_screen_test_" + std::to_string(getpid()) + ".db";

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }

        eventBus = std::make_unique<EventBus>();
        database = std::make_unique<Database>(testDbPath);
        database->initialize();
        deviceManager = std::make_unique<DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
        automationManager = std::make_unique<AutomationManager>(
            *eventBus, *database, *deviceManager);
        automationManager->initialize();
        themeManager = std::make_unique<ThemeManager>();
        uiManager = std::make_unique<UIManager>(*eventBus, *deviceManager);

        // Note: We don't initialize UIManager (no display in CI)
        // but screens can still be constructed for testing
    }

    void TearDown() override {
        screenManager.reset();
        uiManager.reset();
        themeManager.reset();
        automationManager.reset();
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
    std::unique_ptr<AutomationManager> automationManager;
    std::unique_ptr<ThemeManager> themeManager;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<ScreenManager> screenManager;
};

// ============================================================================
// AutomationListScreen Tests
// ============================================================================

TEST_F(AutomationScreenTest, AutomationListScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);

    EXPECT_EQ(automationList->name(), "automations");

    screenManager->registerScreen("automations", std::move(automationList));
    EXPECT_TRUE(screenManager->hasScreen("automations"));
}

TEST_F(AutomationScreenTest, AutomationListScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto* automationListPtr = automationList.get();

    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("automations", std::move(automationList));

    screenManager->showScreen("settings");
    screenManager->showScreen("automations");

    EXPECT_EQ(screenManager->currentScreen(), automationListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);
}

TEST_F(AutomationScreenTest, AutomationListScreenUpdate) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);

    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->showScreen("automations");

    // Multiple updates should not crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(100);
        });
    }
}

TEST_F(AutomationScreenTest, AutomationListScreenBackNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto* settingsPtr = settings.get();

    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("automations", std::move(automationList));

    screenManager->showScreen("settings");
    screenManager->showScreen("automations");

    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate back
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), settingsPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

// ============================================================================
// AddAutomationScreen Tests
// ============================================================================

TEST_F(AutomationScreenTest, AddAutomationScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    EXPECT_EQ(addAutomation->name(), "add_automation");

    screenManager->registerScreen("add_automation", std::move(addAutomation));
    EXPECT_TRUE(screenManager->hasScreen("add_automation"));
}

TEST_F(AutomationScreenTest, AddAutomationScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);
    auto* addAutomationPtr = addAutomation.get();

    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->registerScreen("add_automation", std::move(addAutomation));

    screenManager->showScreen("automations");
    screenManager->showScreen("add_automation");

    EXPECT_EQ(screenManager->currentScreen(), addAutomationPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);
}

TEST_F(AutomationScreenTest, AddAutomationScreenUpdate) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    screenManager->registerScreen("add_automation", std::move(addAutomation));
    screenManager->showScreen("add_automation");

    // Multiple updates should not crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(100);
        });
    }
}

TEST_F(AutomationScreenTest, AddAutomationScreenMultiStepWizard) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    // Total steps should be 4
    EXPECT_EQ(AddAutomationScreen::TOTAL_STEPS, 4);

    screenManager->registerScreen("add_automation", std::move(addAutomation));
    EXPECT_TRUE(screenManager->hasScreen("add_automation"));
}

TEST_F(AutomationScreenTest, AddAutomationScreenBackNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);
    auto* automationListPtr = automationList.get();

    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->registerScreen("add_automation", std::move(addAutomation));

    screenManager->showScreen("automations");
    screenManager->showScreen("add_automation");

    // Navigate back
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), automationListPtr);
}

// ============================================================================
// EditAutomationScreen Tests
// ============================================================================

TEST_F(AutomationScreenTest, EditAutomationScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto editAutomation = std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    EXPECT_EQ(editAutomation->name(), "edit_automation");

    screenManager->registerScreen("edit_automation", std::move(editAutomation));
    EXPECT_TRUE(screenManager->hasScreen("edit_automation"));
}

TEST_F(AutomationScreenTest, EditAutomationScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto editAutomation = std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);
    auto* editAutomationPtr = editAutomation.get();

    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->registerScreen("edit_automation", std::move(editAutomation));

    screenManager->showScreen("automations");
    screenManager->showScreen("edit_automation");

    EXPECT_EQ(screenManager->currentScreen(), editAutomationPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);
}

TEST_F(AutomationScreenTest, EditAutomationScreenUpdate) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto editAutomation = std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    screenManager->registerScreen("edit_automation", std::move(editAutomation));
    screenManager->showScreen("edit_automation");

    // Multiple updates should not crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(100);
        });
    }
}

TEST_F(AutomationScreenTest, EditAutomationScreenSetAutomationId) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto editAutomation = std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    editAutomation->setAutomationId("test_auto_001");

    screenManager->registerScreen("edit_automation", std::move(editAutomation));
    EXPECT_TRUE(screenManager->hasScreen("edit_automation"));
}

TEST_F(AutomationScreenTest, EditAutomationScreenBackNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto editAutomation = std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);
    auto* automationListPtr = automationList.get();

    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->registerScreen("edit_automation", std::move(editAutomation));

    screenManager->showScreen("automations");
    screenManager->showScreen("edit_automation");

    // Navigate back
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), automationListPtr);
}

// ============================================================================
// Full Automation Navigation Flow Tests
// ============================================================================

TEST_F(AutomationScreenTest, FullAutomationNavigationFlow) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);
    auto editAutomation = std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    auto* dashPtr = dashboard.get();
    auto* settingsPtr = settings.get();
    auto* automationListPtr = automationList.get();
    auto* addAutomationPtr = addAutomation.get();
    auto* editAutomationPtr = editAutomation.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->registerScreen("add_automation", std::move(addAutomation));
    screenManager->registerScreen("edit_automation", std::move(editAutomation));
    screenManager->setHomeScreen("dashboard");

    // Start at dashboard
    screenManager->showScreen("dashboard");
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Navigate to settings
    screenManager->showScreen("settings");
    EXPECT_EQ(screenManager->currentScreen(), settingsPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate to automation list
    screenManager->showScreen("automations");
    EXPECT_EQ(screenManager->currentScreen(), automationListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Navigate to add automation
    screenManager->showScreen("add_automation");
    EXPECT_EQ(screenManager->currentScreen(), addAutomationPtr);
    EXPECT_EQ(screenManager->stackDepth(), 3);

    // Go back to automation list
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), automationListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Navigate to edit automation
    screenManager->showScreen("edit_automation");
    EXPECT_EQ(screenManager->currentScreen(), editAutomationPtr);
    EXPECT_EQ(screenManager->stackDepth(), 3);

    // Go home
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(AutomationScreenTest, AutomationScreensMultipleShowHide) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto automationList = std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager);
    auto addAutomation = std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager);

    screenManager->registerScreen("automations", std::move(automationList));
    screenManager->registerScreen("add_automation", std::move(addAutomation));

    // Show/hide multiple times
    for (int i = 0; i < 3; i++) {
        screenManager->showScreen("automations");
        screenManager->showScreen("add_automation");
        screenManager->goBack();
    }

    // Should not crash and end at automations list
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

#else // !SMARTHUB_ENABLE_LVGL

// Placeholder tests when LVGL is not available
TEST(AutomationScreenTest, LVGLNotAvailable) {
    GTEST_SKIP() << "LVGL not available, Automation Screen tests skipped";
}

#endif // SMARTHUB_ENABLE_LVGL
