/**
 * Screen Manager Implementation
 */

#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/Screen.hpp"
#include "smarthub/ui/UIManager.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

ScreenManager::ScreenManager(UIManager& uiManager)
    : m_uiManager(uiManager)
{
}

ScreenManager::~ScreenManager() {
    // Destroy all screens in reverse registration order
    for (auto& [name, screen] : m_screens) {
        if (screen) {
            if (screen->state() != ScreenState::Destroyed) {
                screen->onDestroy();
                screen->setState(ScreenState::Destroyed);
            }
        }
    }
    m_screens.clear();
}

void ScreenManager::registerScreen(const std::string& name, std::unique_ptr<Screen> screen) {
    if (!screen) {
        LOG_ERROR("Cannot register null screen: %s", name.c_str());
        return;
    }

    if (m_screens.count(name) > 0) {
        LOG_WARN("Screen already registered, replacing: %s", name.c_str());
        unregisterScreen(name);
    }

    LOG_DEBUG("Registering screen: %s", name.c_str());
    m_screens[name] = std::move(screen);

#ifdef SMARTHUB_ENABLE_LVGL
    // Create container for this screen
    Screen* scr = m_screens[name].get();
    lv_obj_t* container = lv_obj_create(nullptr);
    lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    // Set a default dark background to prevent black screen flash
    // Individual screens may override this in onCreate()
    lv_obj_set_style_bg_color(container, lv_color_hex(0x121212), 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);  // Prevent screen-level scrolling
    scr->setContainer(container);
#endif

    // Call onCreate
    m_screens[name]->onCreate();
}

bool ScreenManager::unregisterScreen(const std::string& name) {
    auto it = m_screens.find(name);
    if (it == m_screens.end()) {
        return false;
    }

    Screen* screen = it->second.get();

    // Hide if currently visible
    if (screen->isVisible()) {
        screen->onHide();
        screen->setState(ScreenState::Hidden);
    }

    // Destroy
    if (screen->state() != ScreenState::Destroyed) {
        screen->onDestroy();
        screen->setState(ScreenState::Destroyed);
    }

    m_screens.erase(it);

    // Clear from history
    std::stack<std::string> newHistory;
    while (!m_history.empty()) {
        if (m_history.top() != name) {
            newHistory.push(m_history.top());
        }
        m_history.pop();
    }
    // Reverse to maintain order
    while (!newHistory.empty()) {
        m_history.push(newHistory.top());
        newHistory.pop();
    }

    if (m_currentScreen == name) {
        m_currentScreen.clear();
    }

    return true;
}

bool ScreenManager::showScreen(const std::string& name, TransitionType transition, bool pushToStack) {
    // Block navigation during transitions to prevent touch interference
    if (m_transitionInProgress) {
        LOG_DEBUG("Ignoring navigation to %s - transition in progress", name.c_str());
        return false;
    }

    auto it = m_screens.find(name);
    if (it == m_screens.end()) {
        LOG_ERROR("Screen not found: %s", name.c_str());
        return false;
    }

    Screen* newScreen = it->second.get();
    Screen* oldScreen = currentScreen();

    // Don't navigate to the same screen
    if (m_currentScreen == name) {
        LOG_DEBUG("Already on screen: %s", name.c_str());
        return true;
    }

    // Push current screen to history if requested
    if (pushToStack && !m_currentScreen.empty()) {
        m_history.push(m_currentScreen);
    }

    // Hide old screen
    if (oldScreen && oldScreen->isVisible()) {
        oldScreen->onHide();
        oldScreen->setState(ScreenState::Hidden);
    }

    // Perform transition
    performTransition(oldScreen, newScreen, transition);

    // Show new screen
    m_currentScreen = name;
    newScreen->onShow();
    newScreen->setState(ScreenState::Visible);

    LOG_DEBUG("Navigated to screen: %s (stack depth: %zu)", name.c_str(), m_history.size());
    return true;
}

bool ScreenManager::goBack(TransitionType transition) {
    // Block during transitions
    if (m_transitionInProgress) {
        LOG_DEBUG("Ignoring goBack - transition in progress");
        return false;
    }

    if (m_history.empty()) {
        LOG_DEBUG("Navigation history empty, cannot go back");
        return false;
    }

    std::string previousScreen = m_history.top();
    m_history.pop();

    // Show previous screen without pushing current to stack
    return showScreen(previousScreen, transition, false);
}

void ScreenManager::goHome(TransitionType transition) {
    // Block during transitions
    if (m_transitionInProgress) {
        LOG_DEBUG("Ignoring goHome - transition in progress");
        return;
    }

    if (m_homeScreen.empty() || !hasScreen(m_homeScreen)) {
        LOG_WARN("Home screen not set or not found");
        return;
    }

    // Clear history and go to home
    clearHistory();
    showScreen(m_homeScreen, transition, false);
}

Screen* ScreenManager::currentScreen() const {
    if (m_currentScreen.empty()) {
        return nullptr;
    }
    auto it = m_screens.find(m_currentScreen);
    return (it != m_screens.end()) ? it->second.get() : nullptr;
}

Screen* ScreenManager::getScreen(const std::string& name) const {
    auto it = m_screens.find(name);
    return (it != m_screens.end()) ? it->second.get() : nullptr;
}

bool ScreenManager::hasScreen(const std::string& name) const {
    return m_screens.count(name) > 0;
}

void ScreenManager::clearHistory() {
    while (!m_history.empty()) {
        m_history.pop();
    }
}

void ScreenManager::update(uint32_t deltaMs) {
    // Track transition completion
    if (m_transitionInProgress) {
        if (deltaMs >= m_transitionRemainingMs) {
            m_transitionInProgress = false;
            m_transitionRemainingMs = 0;
            LOG_DEBUG("Screen transition completed");
        } else {
            m_transitionRemainingMs -= deltaMs;
        }
    }

    Screen* screen = currentScreen();
    if (screen && screen->isVisible()) {
        screen->onUpdate(deltaMs);
    }
}

void ScreenManager::performTransition(Screen* from, Screen* to, TransitionType type) {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!to || !to->container()) {
        return;
    }

    lv_scr_load_anim_t animType = LV_SCR_LOAD_ANIM_NONE;
    uint32_t duration = m_transitionDuration;

    switch (type) {
        case TransitionType::None:
            animType = LV_SCR_LOAD_ANIM_NONE;
            duration = 0;
            break;
        case TransitionType::Fade:
            animType = LV_SCR_LOAD_ANIM_FADE_IN;
            break;
        case TransitionType::SlideLeft:
            animType = LV_SCR_LOAD_ANIM_MOVE_LEFT;
            break;
        case TransitionType::SlideRight:
            animType = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
            break;
        case TransitionType::SlideUp:
            animType = LV_SCR_LOAD_ANIM_MOVE_TOP;
            break;
        case TransitionType::SlideDown:
            animType = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
            break;
    }

    // Start transition timer to block input during animation
    if (duration > 0) {
        m_transitionInProgress = true;
        m_transitionRemainingMs = duration;
    }

    // Load the new screen with animation
    // auto_del = false because we manage screen lifecycle ourselves
    lv_scr_load_anim(to->container(), animType, duration, 0, false);

    // Force a full screen refresh to clear any transition artifacts
    // This is especially important when using manual display rotation
    lv_obj_invalidate(to->container());
#else
    (void)from;
    (void)to;
    (void)type;
#endif
}

} // namespace ui
} // namespace smarthub
