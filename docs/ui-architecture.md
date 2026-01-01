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

## Widgets

### TimeSeriesChart (`src/ui/widgets/TimeSeriesChart.cpp`)

LVGL chart wrapper for time series data:
- Line chart with configurable Y-axis range
- Up to 60 data points displayed
- Time range dropdown selector
- DataPoint struct with timestamp and value
- Automatic resampling of data to fit chart

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
    +---> Settings (planned)
```

**Tab Navigation**: Direct screen switch without history push
**Detail Navigation**: Pushes to history stack, enables back button

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

### Hardware Tests
Test on STM32MP157F-DK2:
- Touch responsiveness
- Screen transitions
- Theme switching
- Display refresh rate

## File Structure

```
app/
├── include/smarthub/ui/
│   ├── Screen.hpp
│   ├── ScreenManager.hpp
│   ├── ThemeManager.hpp
│   ├── UIManager.hpp
│   ├── screens/
│   │   ├── DashboardScreen.hpp
│   │   ├── DeviceListScreen.hpp
│   │   ├── LightControlScreen.hpp
│   │   ├── SensorListScreen.hpp
│   │   └── SensorHistoryScreen.hpp
│   └── widgets/
│       ├── Header.hpp
│       ├── NavBar.hpp
│       ├── RoomCard.hpp
│       └── TimeSeriesChart.hpp
├── src/ui/
│   ├── Screen.cpp
│   ├── ScreenManager.cpp
│   ├── ThemeManager.cpp
│   ├── UIManager.cpp
│   ├── screens/
│   │   ├── DashboardScreen.cpp
│   │   ├── DeviceListScreen.cpp
│   │   ├── LightControlScreen.cpp
│   │   ├── SensorListScreen.cpp
│   │   └── SensorHistoryScreen.cpp
│   └── widgets/
│       ├── Header.cpp
│       ├── NavBar.cpp
│       ├── RoomCard.cpp
│       └── TimeSeriesChart.cpp
└── tests/ui/
    ├── test_screen_manager.cpp
    ├── test_screens.cpp
    ├── test_theme_manager.cpp
    ├── test_ui.cpp
    └── test_widgets.cpp
```

## Future Work (Phase 8.E-8.G)

- **8.E**: WiFi configuration wizard
- **8.F**: Settings screens, display management
- **8.G**: Animations and polish
