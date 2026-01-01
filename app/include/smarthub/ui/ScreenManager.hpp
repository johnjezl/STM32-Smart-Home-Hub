/**
 * Screen Manager
 *
 * Manages screen navigation, transitions, and lifecycle for the UI.
 * Maintains a navigation stack for back navigation support.
 */
#pragma once

#include <string>
#include <memory>
#include <map>
#include <stack>
#include <functional>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {

class UIManager;

namespace ui {

class Screen;

/**
 * Screen transition animation types
 */
enum class TransitionType {
    None,           // Instant switch
    Fade,           // Fade in/out
    SlideLeft,      // New screen slides in from right
    SlideRight,     // New screen slides in from left
    SlideUp,        // New screen slides in from bottom
    SlideDown       // New screen slides in from top
};

/**
 * Screen Manager - handles navigation between screens
 *
 * Usage:
 *   ScreenManager mgr(uiManager);
 *   mgr.registerScreen("home", std::make_unique<HomeScreen>(mgr, "home"));
 *   mgr.registerScreen("settings", std::make_unique<SettingsScreen>(mgr, "settings"));
 *   mgr.showScreen("home");
 *   // Later...
 *   mgr.showScreen("settings");  // Pushes to stack
 *   mgr.goBack();                // Returns to home
 */
class ScreenManager {
public:
    /**
     * Construct a screen manager
     * @param uiManager Reference to the UI manager
     */
    explicit ScreenManager(UIManager& uiManager);
    ~ScreenManager();

    // Non-copyable
    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    /**
     * Register a screen with the manager
     * @param name Unique screen identifier
     * @param screen Screen instance (ownership transferred)
     */
    void registerScreen(const std::string& name, std::unique_ptr<Screen> screen);

    /**
     * Unregister and destroy a screen
     * @param name Screen identifier
     * @return true if screen was found and removed
     */
    bool unregisterScreen(const std::string& name);

    /**
     * Navigate to a screen
     * @param name Screen identifier
     * @param transition Transition animation type
     * @param pushToStack If true, current screen is pushed to history
     * @return true if navigation succeeded
     */
    bool showScreen(const std::string& name,
                    TransitionType transition = TransitionType::SlideLeft,
                    bool pushToStack = true);

    /**
     * Go back to the previous screen in history
     * @param transition Transition animation (default: slide right)
     * @return true if there was a screen to go back to
     */
    bool goBack(TransitionType transition = TransitionType::SlideRight);

    /**
     * Go to the home screen, clearing navigation history
     * @param transition Transition animation
     */
    void goHome(TransitionType transition = TransitionType::Fade);

    /**
     * Get the currently visible screen
     * @return Pointer to current screen, or nullptr if none
     */
    Screen* currentScreen() const;

    /**
     * Get a registered screen by name
     * @param name Screen identifier
     * @return Pointer to screen, or nullptr if not found
     */
    Screen* getScreen(const std::string& name) const;

    /**
     * Check if a screen is registered
     */
    bool hasScreen(const std::string& name) const;

    /**
     * Get the home screen name
     */
    const std::string& homeScreenName() const { return m_homeScreen; }

    /**
     * Set the home screen name
     * @param name Screen identifier to use as home
     */
    void setHomeScreen(const std::string& name) { m_homeScreen = name; }

    /**
     * Get the navigation stack depth
     */
    size_t stackDepth() const { return m_history.size(); }

    /**
     * Clear navigation history (keeps current screen)
     */
    void clearHistory();

    /**
     * Update the current screen (called each frame)
     * @param deltaMs Milliseconds since last update
     */
    void update(uint32_t deltaMs);

    /**
     * Get reference to UIManager
     */
    UIManager& uiManager() { return m_uiManager; }

    /**
     * Set transition duration in milliseconds
     */
    void setTransitionDuration(uint32_t ms) { m_transitionDuration = ms; }
    uint32_t transitionDuration() const { return m_transitionDuration; }

private:
    void performTransition(Screen* from, Screen* to, TransitionType type);

    UIManager& m_uiManager;
    std::map<std::string, std::unique_ptr<Screen>> m_screens;
    std::stack<std::string> m_history;
    std::string m_currentScreen;
    std::string m_homeScreen = "home";
    uint32_t m_transitionDuration = 300;  // ms
};

} // namespace ui
} // namespace smarthub
