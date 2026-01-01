# Phase 8: UI Refinement & Polish - Implementation Plan

## Current State Analysis

### Existing Infrastructure
- **UIManager**: DRM display backend with double-buffering, evdev touch input
- **LVGL Integration**: Conditional compilation, pkg-config detection
- **Display**: 800x480 landscape, DSI MIPI
- **Test Framework**: Basic UIManager tests (10 tests)

### Dependencies Already Met
- Phases 4-7 complete (Zigbee, WiFi, M4, Security)
- DeviceManager with device registry and state management
- EventBus for component communication
- Database for persistence

---

## Implementation Phases

### 8.A: Core UI Architecture (Foundation) ✅ COMPLETE

**Goal**: Establish screen management and navigation framework

**Status**: Implemented and tested

#### Files Created
```
app/include/smarthub/ui/
├── Screen.hpp              # Base screen class
├── ScreenManager.hpp       # Navigation controller
├── ThemeManager.hpp        # Theme/styling system
└── widgets/
    └── Widget.hpp          # Base widget class

app/src/ui/
├── Screen.cpp
├── ScreenManager.cpp
└── ThemeManager.cpp
```

#### Key Components
1. **Screen Base Class**
   - Lifecycle: onCreate, onShow, onHide, onDestroy, onUpdate
   - Container management (lv_obj_t*)
   - Reference to UIManager

2. **ScreenManager**
   - Screen registry (name → Screen*)
   - Navigation stack (history)
   - Transitions (slide, fade)
   - goBack(), goHome(), showScreen()

3. **ThemeManager**
   - Light/Dark themes
   - Color palette (background, surface, primary, text)
   - Font management
   - Style presets

#### Tests
- `test_screen_manager.cpp`: 14 tests for navigation and lifecycle
- `test_theme_manager.cpp`: 8 tests for theme colors and modes

---

### 8.B: Dashboard & Navigation (Primary Screen) ✅ COMPLETE

**Goal**: Main home screen users see after boot

**Status**: Implemented and tested

#### Files Created
```
app/include/smarthub/ui/screens/
├── DashboardScreen.hpp
└── SettingsScreen.hpp

app/include/smarthub/ui/widgets/
├── RoomCard.hpp
├── NavBar.hpp
└── Header.hpp

app/src/ui/screens/
├── DashboardScreen.cpp
└── SettingsScreen.cpp

app/src/ui/widgets/
├── RoomCard.cpp
├── NavBar.cpp
└── Header.cpp
```

#### Dashboard Layout (800x480 Landscape)
```
+---------------------------------------------------------------+
|  SmartHub                                    🔔  ⚙️  12:34 PM |  <- Header (50px)
+---------------------------------------------------------------+
|                                                               |
|  +-------------+ +-------------+ +-------------+ +----------+ |
|  | Living Rm   | | Bedroom     | | Kitchen     | | Bathroom | |  <- Room Cards
|  | 72°F   💡3  | | 68°F   💡2  | | 74°F   💡1  | | 70°F 💡0 | |     (scrollable)
|  +-------------+ +-------------+ +-------------+ +----------+ |
|                                                               |
|  +-------------+ +-------------+ +-------------+ +----------+ |
|  | Garage      | | Office      | | Patio       | | + Add    | |
|  | --°F   💡1  | | 71°F   💡2  | | 65°F   💡0  | |          | |
|  +-------------+ +-------------+ +-------------+ +----------+ |
|                                                               |
+---------------------------------------------------------------+
|    [🏠 Home]    [💡 Devices]    [📊 Sensors]    [⚙️ Settings] |  <- NavBar (60px)
+---------------------------------------------------------------+
```

#### Components
1. **Header** - Title, time, notification/settings icons
2. **RoomCard** - Room name, temperature, active device count
3. **NavBar** - Bottom navigation (3-4 buttons)
4. **DashboardScreen** - Composes above, handles room grid

#### Tests
- `test_widgets.cpp`: NavTab, RoomData structure tests, widget constants

---

### 8.C: Device Control Screens ✅ COMPLETE

**Goal**: Control individual devices

**Status**: Implemented and tested

#### Files Created
```
app/include/smarthub/ui/screens/
├── DeviceListScreen.hpp
├── LightControlScreen.hpp
├── ThermostatScreen.hpp
└── SensorDetailScreen.hpp

app/src/ui/screens/
├── DeviceListScreen.cpp
├── LightControlScreen.cpp
├── ThermostatScreen.cpp
└── SensorDetailScreen.cpp
```

#### Light Control Features
- Power toggle (large switch)
- Brightness slider (0-100%)
- Color temperature slider (warm/cool)
- Color picker (for RGB lights)

#### Thermostat Features
- Temperature arc display
- Setpoint adjustment (+/- buttons)
- Mode selection (Heat/Cool/Auto/Off)

#### Tests
- `test_screens.cpp`: Screen registration, navigation, lifecycle tests
- Full navigation flow tests (dashboard → devices → light control → back)
- Tab navigation tests (no history push)

---

### 8.D: Sensor Displays ⏳ PENDING

**Goal**: Show sensor readings and history

#### Files to Create
```
app/include/smarthub/ui/screens/
├── SensorOverviewScreen.hpp
└── SensorHistoryScreen.hpp

app/include/smarthub/ui/widgets/
├── SensorCard.hpp
└── TimeSeriesChart.hpp
```

#### Features
- Current readings with icons (temp, humidity, motion)
- LVGL chart widget for history (1h, 24h, 7d views)
- Time range selector dropdown

---

### 8.E: WiFi Configuration Wizard

**Goal**: First-run WiFi setup with on-screen keyboard

#### Files to Create
```
app/include/smarthub/ui/screens/
├── WifiSetupScreen.hpp
└── FirstRunWizard.hpp

app/src/ui/screens/
├── WifiSetupScreen.cpp
└── FirstRunWizard.cpp
```

#### Features
- Network list (scan results)
- Signal strength indicators
- Password dialog with virtual keyboard
- Connection status/spinner
- Error handling with retry

---

### 8.F: Settings & Display Management

**Goal**: User-configurable options

#### Settings Categories
1. **Network** - WiFi, IP settings
2. **Display** - Brightness, theme, timeout
3. **Devices** - Manage paired devices
4. **Security** - Change password, API tokens
5. **About** - Version, system info

#### Display Management
- Screen timeout with dimming
- Wake on touch
- Brightness control via sysfs

---

### 8.G: Animations & Polish

**Goal**: Smooth, responsive UI

#### Animations
- Screen transitions (slide left/right, fade)
- Button press feedback (scale 100→95)
- Loading spinners
- Value change animations

#### Polish
- Minimum 48x48 touch targets
- Consistent spacing/margins
- Error states and feedback
- High contrast mode option

---

## Implementation Order

| Order | Component | Depends On | Status |
|-------|-----------|------------|--------|
| 1 | Screen/ScreenManager | UIManager | ✅ Complete |
| 2 | ThemeManager | Screen | ✅ Complete |
| 3 | Header/NavBar widgets | Theme | ✅ Complete |
| 4 | DashboardScreen | Above | ✅ Complete |
| 5 | RoomCard widget | Dashboard | ✅ Complete |
| 6 | DeviceListScreen | Screen | ✅ Complete |
| 7 | LightControlScreen | DeviceList | ✅ Complete |
| 8 | SensorListScreen | Screen | ✅ Complete |
| 9 | SensorHistoryScreen | Chart widget | ⏳ Pending |
| 10 | WifiSetupScreen | Screen, Keyboard | ⏳ Pending |
| 11 | SettingsScreen | Screen | ⏳ Pending |
| 12 | DisplayManager | Theme | ⏳ Pending |
| 13 | Animations | All screens | ⏳ Pending |
| 14 | Testing & Polish | All | ⏳ Pending |

**Progress**: 8/14 components complete (Phase 8.A-8.C)

---

## Testing Strategy

### Unit Tests
- Screen lifecycle management
- Navigation stack (push/pop/home)
- Theme color values
- Widget state updates

### Integration Tests
- Screen transitions
- Device state → UI updates
- Touch event handling
- Full navigation flows

### Hardware Tests (on STM32MP157F-DK2)
- Touch responsiveness (<100ms)
- Frame rate (target 30+ FPS)
- Display brightness control
- Screen timeout/wake

---

## Asset Requirements

### Fonts
- Roboto Regular: 16, 24, 32, 48px
- Material Design Icons subset

### Icons Needed
- Lightbulb (on/off states)
- Thermometer
- Humidity drop
- Motion sensor
- WiFi (signal levels)
- Settings gear
- Power
- Chevrons (navigation)
- Plus/minus
- Home

---

## Open Questions

1. **Rooms**: How are rooms/zones defined? Config file or UI?
2. **First-Run**: Should first-run wizard include device pairing?
3. **Notifications**: What events trigger the notification indicator?
4. **Localization**: English only or plan for i18n?

---

## Success Criteria

- [x] Screen base class with lifecycle management
- [x] ScreenManager with navigation stack
- [x] ThemeManager with light/dark modes
- [x] Dashboard screen with room cards
- [x] Device list screen with toggle switches
- [x] Light control screen (power, brightness, color temp)
- [x] Sensor list screen with readings
- [x] Comprehensive unit tests (30+ tests)
- [ ] All screens navigable via touch (no keyboard)
- [ ] Touch response <100ms
- [ ] UI renders at 30+ FPS
- [ ] Can configure WiFi from first boot
- [ ] Device state changes reflected in UI within 500ms
- [ ] Theme switching works (light/dark)
- [ ] Screen timeout/wake functions correctly
