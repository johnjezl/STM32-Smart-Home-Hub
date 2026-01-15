# SmartHub UI Architecture

This document describes the UI architecture implemented in Phase 8 of the SmartHub project.

## Overview

The SmartHub UI is built on LVGL (Light and Versatile Graphics Library) running on the STM32MP157F-DK2 discovery board with a 4.3" capacitive touchscreen (800x480 landscape orientation).

## Architecture Diagram

```
+-------------------+     +------------------+     +------------------+
|    UIManager      |---->|  ScreenManager   |---->|     Screen       |
| (DRM, Touch, LVGL)|     | (Navigation)     |     | (Base Class)     |
+-------------------+     +------------------+     +------------------+
         |                        |                        |
         v                        v                        v
+-------------------+     +------------------+     +------------------+
|   ThemeManager    |     |  Screen Stack    |     | DashboardScreen  |
| (Light/Dark)      |     | (History)        |     | DeviceListScreen |
+-------------------+     +------------------+     | SensorListScreen |
                                                   | LightControlScr  |
                                                   +------------------+
```

## Core Components

### UIManager (`src/ui/UIManager.cpp`)

The UIManager is responsible for:
- DRM display initialization and double-buffering
- Touch input via evdev
- LVGL timer handler execution
- Display dimensions (800x480 landscape)

### ScreenManager (`src/ui/ScreenManager.cpp`)

The ScreenManager handles:
- Screen registration by name
- Navigation with history stack
- Screen transitions (slide, fade)
- Lifecycle callbacks (show, hide)

**Key APIs:**
```cpp
void registerScreen(const std::string& name, std::unique_ptr<Screen> screen);
bool showScreen(const std::string& name, TransitionType type, bool pushToStack);
bool goBack(TransitionType type);
void goHome(TransitionType type);
void update(uint32_t deltaMs);
```

### Screen (`src/ui/Screen.cpp`)

Base class for all screens with lifecycle management:

```cpp
class Screen {
public:
    virtual void onCreate() = 0;   // Called once after registration
    virtual void onShow() {}       // Called when becoming visible
    virtual void onHide() {}       // Called when leaving screen
    virtual void onDestroy() {}    // Called before unregistration
    virtual void onUpdate(uint32_t deltaMs) {}  // Called each frame
};
```

### ThemeManager (`src/ui/ThemeManager.cpp`)

Manages light/dark themes with Material Design-inspired color palette:

**Light Theme:**
| Color | Hex | Usage |
|-------|-----|-------|
| Background | #F5F5F5 | Screen background |
| Surface | #FFFFFF | Cards, dialogs |
| Primary | #2196F3 | Buttons, active states |
| Text Primary | #212121 | Main text |
| Text Secondary | #757575 | Subtitles |

**Dark Theme:**
| Color | Hex | Usage |
|-------|-----|-------|
| Background | #121212 | Screen background |
| Surface | #1E1E1E | Cards, dialogs |
| Primary | #2196F3 | Buttons, active states |
| Text Primary | #FFFFFF | Main text |
| Text Secondary | #B3B3B3 | Subtitles |

## Widgets

### Header (`src/ui/widgets/Header.cpp`)

Top bar widget (50px height):
- Title text (left-aligned)
- Notification button with badge
- Settings button
- Clock display

### NavBar (`src/ui/widgets/NavBar.cpp`)

Bottom navigation bar (60px height):
- Tab-based navigation
- Icon + label for each tab
- Active state highlighting

Tabs: Home | Devices | Sensors | Settings

### RoomCard (`src/ui/widgets/RoomCard.cpp`)

Dashboard room summary card (180x100px):
- Room name
- Temperature (if sensor available)
- Active device count with icon

## Screens

### DashboardScreen (`src/ui/screens/DashboardScreen.cpp`)

Main home screen layout:
```
+---------------------------------------------------------------+
|  SmartHub                                    🔔  ⚙️  12:34 PM |  <- Header
+---------------------------------------------------------------+
|                                                               |
|  +-------------+ +-------------+ +-------------+ +----------+ |
|  | Living Rm   | | Bedroom     | | Kitchen     | | Bathroom | |  <- RoomCards
|  | 72°F   💡3  | | 68°F   💡2  | | 74°F   💡1  | | 70°F 💡0 | |
|  +-------------+ +-------------+ +-------------+ +----------+ |
|                                                               |
+---------------------------------------------------------------+
|    [🏠 Home]    [💡 Devices]    [📊 Sensors]    [⚙️ Settings] |  <- NavBar
+---------------------------------------------------------------+
```

### DeviceListScreen (`src/ui/screens/DeviceListScreen.cpp`)

Scrollable list of all devices:
- Device icon (based on type)
- Device name and type
- Power toggle switch
- Click to open control screen

### LightControlScreen (`src/ui/screens/LightControlScreen.cpp`)

Detailed light control:
- Preview circle (shows current state)
- Power toggle switch
- Brightness slider (0-100%)
- Color temperature slider (2700K-6500K)
- Back navigation button

### SensorListScreen (`src/ui/screens/SensorListScreen.cpp`)

Grid of sensor cards:
- Temperature sensors (°F)
- Humidity sensors (%)
- Motion sensors (Active/Inactive)
- Contact sensors (Open/Closed)
- Auto-refresh every 5 seconds
- Click card to view history

### SensorHistoryScreen (`src/ui/screens/SensorHistoryScreen.cpp`)

Sensor history with time series chart:
- Back navigation button
- Sensor name and current value display
- TimeSeriesChart widget with historical data
- Time range selector (1h, 6h, 24h, 7d)
- Auto-refresh every minute

### WifiSetupScreen (`src/ui/screens/WifiSetupScreen.cpp`)

WiFi network configuration:
- Header with back button and refresh
- Status bar showing connection state and IP
- Scrollable list of available networks
- Signal strength indicators (4-bar visual)
- Security type and lock icon display
- Password dialog with LVGL virtual keyboard
- Show/hide password toggle
- Loading spinner during scan/connect
- Error message display
- Auto-refresh every 30 seconds

## Widgets

### TimeSeriesChart (`src/ui/widgets/TimeSeriesChart.cpp`)

LVGL chart wrapper for time series data:
- Line chart with configurable Y-axis range
- Up to 60 data points displayed
- Time range dropdown selector
- DataPoint struct with timestamp and value
- Automatic resampling of data to fit chart

## Network Management

### NetworkManager (`src/network/NetworkManager.cpp`)

WiFi network management using nmcli (NetworkManager CLI):
- Scan for available WiFi networks
- Connect to networks with password
- Disconnect from current network
- Get connection status and IP address
- Signal strength conversion (dBm to percentage, icon levels)
- Async operations with callbacks

Key structures:
- `WifiNetwork`: SSID, BSSID, signal strength, security type
- `ConnectionState`: Disconnected, Scanning, Connecting, Connected, Failed
- `ConnectionResult`: Success/failure with error message and IP
- `NetworkStatus`: Current connection state and details

### SettingsScreen (`src/ui/screens/SettingsScreen.cpp`)

Main settings menu with category list:
- Network (links to WifiSetupScreen)
- Display (links to DisplaySettingsScreen)
- Devices (manage paired devices)
- Security (password, API tokens)
- About (system information)

Each category shows icon, title, subtitle, and chevron indicator.

### DisplaySettingsScreen (`src/ui/screens/DisplaySettingsScreen.cpp`)

Display and theme settings:
- Brightness slider (10-100%)
- Screen timeout buttons (Never, 30s, 1m, 5m, 10m)
- Theme buttons (Light, Dark, Auto)
- Back navigation button

Uses DisplayManager for sysfs backlight control.

### AboutScreen (`src/ui/screens/AboutScreen.cpp`)

System information display:
- Version and build date
- Platform (STM32MP157F-DK2)
- Kernel version from /proc/version
- System uptime from /proc/uptime
- IP and MAC address
- Memory usage with progress bar

## Display Management

### DisplayManager (`src/ui/DisplayManager.cpp`)

Hardware display control via Linux sysfs:
- Brightness control (0-100%) → `/sys/class/backlight/brightness`
- Screen timeout with configurable duration
- Dim phase before screen off (5 second warning)
- Wake on touch event
- Timeout callback notifications

**Key APIs:**
```cpp
void setBrightness(int percent);
void setTimeoutSeconds(int seconds);
void setDimLevel(int percent);
void wake();
void setScreenOn(bool on);
void setTimeoutCallback(TimeoutCallback callback);
```

## Navigation Flow

```
Dashboard (Home)
    |
    +---> Devices
    |       |
    |       +---> Light Control (push to stack)
    |               |
    |               +---> Back (pop from stack)
    |
    +---> Sensors
    |       |
    |       +---> Sensor History (push to stack)
    |               |
    |               +---> Back (pop from stack)
    |
    +---> Settings
            |
            +---> WiFi Setup (push to stack)
            |       |
            |       +---> Back (pop from stack)
            |
            +---> Display Settings (push to stack)
            |       |
            |       +---> Back (pop from stack)
            |
            +---> About (push to stack)
                    |
                    +---> Back (pop from stack)
```

**Tab Navigation**: Direct screen switch without history push
**Detail Navigation**: Pushes to history stack, enables back button

## Thread Safety

LVGL is **not thread-safe**. All LVGL API calls must be made from the main thread (the thread running `lv_timer_handler()`). This is critical when integrating with protocol handlers like Zigbee that run background threads.

### Using lv_async_call()

When callbacks from background threads need to update the UI, use `lv_async_call()` to schedule the update on the main thread:

```cpp
// WRONG - Called from background thread, causes intermittent UI glitches
void MyScreen::onDeviceDiscovered(DevicePtr device) {
    lv_label_set_text(m_statusLabel, "Device found!");  // Not thread-safe!
}

// CORRECT - Defers UI update to main thread
void MyScreen::onDeviceDiscovered(DevicePtr device) {
    // Store data for async callback
    m_discoveredDevice = device;

    // Schedule UI update on main thread
    lv_async_call([](void* userData) {
        auto* self = static_cast<MyScreen*>(userData);
        if (!self) return;

        lv_label_set_text(self->m_statusLabel, "Device found!");
        lv_obj_clear_flag(self->m_spinner, LV_OBJ_FLAG_HIDDEN);
    }, this);
}
```

### Common Sources of Background Threads

These subsystems invoke callbacks from background threads:
- **ZigbeeHandler**: Device discovery, state changes
- **MQTTHandler**: Message reception
- **EventBus**: Async event delivery
- **NetworkManager**: WiFi scan results, connection status

Always wrap UI updates from these callbacks in `lv_async_call()`.

### Best Practices

1. Store callback data in member variables before calling `lv_async_call()`
2. Always check the `userData` pointer for nullptr in the async callback
3. Keep async callbacks short - just update UI state
4. Use `lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN)` / `lv_obj_add_flag()` for show/hide

## Conditional Compilation

All LVGL-dependent code is wrapped with:
```cpp
#ifdef SMARTHUB_ENABLE_LVGL
// LVGL code here
#endif
```

This allows building without LVGL for:
- Unit testing on CI servers
- Native development without display

## Testing

### Unit Tests
- `test_theme_manager.cpp`: Color values, mode switching
- `test_screen_manager.cpp`: Registration, navigation, lifecycle
- `test_widgets.cpp`: Widget constants, data structures
- `test_screens.cpp`: Screen registration, navigation flows
- `test_network.cpp`: NetworkManager, WiFi structs, signal conversion

### Hardware Tests
Test on STM32MP157F-DK2:
- Touch responsiveness
- Screen transitions
- Theme switching
- Display refresh rate

## File Structure

```
app/
├── include/smarthub/
│   ├── network/
│   │   └── NetworkManager.hpp
│   └── ui/
│       ├── AnimationManager.hpp
│       ├── DisplayManager.hpp
│       ├── Screen.hpp
│       ├── ScreenManager.hpp
│       ├── ThemeManager.hpp
│       ├── UIManager.hpp
│       ├── screens/
│       │   ├── AboutScreen.hpp
│       │   ├── DashboardScreen.hpp
│       │   ├── DeviceListScreen.hpp
│       │   ├── DisplaySettingsScreen.hpp
│       │   ├── LightControlScreen.hpp
│       │   ├── SensorHistoryScreen.hpp
│       │   ├── SensorListScreen.hpp
│       │   ├── SettingsScreen.hpp
│       │   └── WifiSetupScreen.hpp
│       └── widgets/
│           ├── Header.hpp
│           ├── LoadingSpinner.hpp
│           ├── NavBar.hpp
│           ├── RoomCard.hpp
│           └── TimeSeriesChart.hpp
├── src/
│   ├── network/
│   │   └── NetworkManager.cpp
│   └── ui/
│       ├── AnimationManager.cpp
│       ├── DisplayManager.cpp
│       ├── Screen.cpp
│       ├── ScreenManager.cpp
│       ├── ThemeManager.cpp
│       ├── UIManager.cpp
│       ├── screens/
│       │   ├── AboutScreen.cpp
│       │   ├── DashboardScreen.cpp
│       │   ├── DeviceListScreen.cpp
│       │   ├── DisplaySettingsScreen.cpp
│       │   ├── LightControlScreen.cpp
│       │   ├── SensorHistoryScreen.cpp
│       │   ├── SensorListScreen.cpp
│       │   ├── SettingsScreen.cpp
│       │   └── WifiSetupScreen.cpp
│       └── widgets/
│           ├── Header.cpp
│           ├── LoadingSpinner.cpp
│           ├── NavBar.cpp
│           ├── RoomCard.cpp
│           └── TimeSeriesChart.cpp
└── tests/
    ├── network/
    │   └── test_network.cpp
    └── ui/
        ├── test_screen_manager.cpp
        ├── test_screens.cpp
        ├── test_theme_manager.cpp
        ├── test_ui.cpp
        └── test_widgets.cpp
```

## Animations (Phase 8.G)

### AnimationManager (`src/ui/AnimationManager.cpp`)

Provides reusable animation utilities:

```cpp
AnimationManager anim;
anim.setupButtonPressEffect(btn);  // Scale 95% + color on press
anim.fadeIn(obj, 300);             // Fade in over 300ms
anim.fadeOut(obj, 300);            // Fade out
anim.pulse(obj, 110);              // Pulse to 110% size
anim.shake(obj, 10);               // Error shake (10px amplitude)
anim.slideTo(obj, x, y);           // Animated move
anim.animateValue(slider, 0, 100); // Animate value change
```

**Animation Durations:**
- DURATION_FAST: 150ms (quick feedback)
- DURATION_NORMAL: 300ms (standard transitions)
- DURATION_SLOW: 500ms (deliberate animations)

**Easing Functions:**
- Linear, EaseOut, EaseIn, EaseInOut, Overshoot, Bounce

### LoadingSpinner (`src/ui/widgets/LoadingSpinner.cpp`)

Animated loading indicator:
- Rotating arc animation
- Configurable size (default 48px)
- Configurable speed (default 1s/rotation)
- Uses theme primary color
- Show/hide controls animation

```cpp
LoadingSpinner spinner(parent, theme, 48);
spinner.show();   // Start animation
spinner.hide();   // Stop animation
spinner.setSpeed(500);  // Faster rotation
```

### High Contrast Mode

Accessibility theme with maximum contrast:
- Pure black background (#000000)
- Pure white text (#FFFFFF)
- Cyan primary (#00FFFF)
- No gray colors
- Pure red/green/yellow for error/success/warning

```cpp
theme.setMode(ThemeMode::HighContrast);
if (theme.isHighContrast()) {
    // Adjust for accessibility
}
```
