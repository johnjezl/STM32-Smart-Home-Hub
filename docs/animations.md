# SmartHub Animation Guide

This document describes the animation system implemented in Phase 8.G of the SmartHub project.

## Overview

The animation system provides smooth, responsive UI feedback through:
- **AnimationManager**: Reusable animation utilities
- **LoadingSpinner**: Animated loading indicator widget
- **ThemeManager enhancements**: Button press effects and state feedback styles

## AnimationManager

The `AnimationManager` class provides a collection of animation utilities for LVGL objects.

### Include

```cpp
#include <smarthub/ui/AnimationManager.hpp>
```

### Duration Constants

| Constant | Value | Use Case |
|----------|-------|----------|
| `DURATION_FAST` | 150ms | Quick feedback (button press, toggle) |
| `DURATION_NORMAL` | 300ms | Standard transitions (screen changes) |
| `DURATION_SLOW` | 500ms | Deliberate animations (loading states) |

### Scale Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `PRESS_SCALE` | 95 | Button pressed state (95% of normal) |
| `NORMAL_SCALE` | 100 | Default scale (100%) |

### Easing Functions

```cpp
enum class AnimationEasing {
    Linear,      // Constant speed
    EaseOut,     // Fast start, slow end (default for most)
    EaseIn,      // Slow start, fast end
    EaseInOut,   // Slow start and end
    Overshoot,   // Goes past target then settles
    Bounce       // Bounces at the end
};
```

### Button Press Effect

Apply a scale-down + color change effect when buttons are pressed:

```cpp
AnimationManager anim;
lv_obj_t* btn = lv_btn_create(parent);

// Apply press effect (scales to 95% and darkens on press)
anim.setupButtonPressEffect(btn);
```

This creates a smooth transition that:
- Scales the button to 95% when pressed
- Darkens the background color
- Animates back to normal on release

### Fade Animations

```cpp
AnimationManager anim;

// Fade in over 300ms with ease-out
anim.fadeIn(obj, 300, AnimationEasing::EaseOut);

// Fade out over 200ms with ease-in
anim.fadeOut(obj, 200, AnimationEasing::EaseIn);

// Quick fade (uses default duration)
anim.fadeIn(obj);
```

### Pulse Animation

Draw attention to an element by scaling it up and back:

```cpp
AnimationManager anim;

// Pulse to 110% size over 300ms
anim.pulse(obj, 110, 300);

// Quick attention pulse (default 110% over DURATION_NORMAL)
anim.pulse(obj);
```

Use cases:
- Notification indicators
- New item highlights
- Successful action feedback

### Shake Animation

Indicate an error with a horizontal shake:

```cpp
AnimationManager anim;

// Shake 10 pixels left/right over 150ms
anim.shake(obj, 10, 150);

// Quick error shake (default values)
anim.shake(obj);
```

Use cases:
- Invalid form input
- Failed authentication
- Action blocked

### Slide Animation

Move an object to a new position with animation:

```cpp
AnimationManager anim;

// Slide to position (100, 200) over 300ms
anim.slideTo(obj, 100, 200, 300, AnimationEasing::EaseOut);

// Slide with default timing
anim.slideTo(obj, 100, 200);
```

### Value Animation

Animate value changes for sliders, progress bars, and arcs:

```cpp
AnimationManager anim;

// Animate slider from 0 to 100
anim.animateValue(slider, 0, 100, 300);

// Animate progress bar
anim.animateValue(progressBar, currentValue, newValue);

// Animate arc (e.g., thermostat display)
anim.animateValue(arc, 68, 72, 500);
```

Automatically detects widget type (slider, bar, arc) and uses the appropriate setter.

## LoadingSpinner

An animated loading indicator using a rotating arc.

### Include

```cpp
#include <smarthub/ui/widgets/LoadingSpinner.hpp>
```

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_SIZE` | 48px | Default spinner diameter |
| `DEFAULT_DURATION` | 1000ms | Time for one full rotation |
| `ARC_LENGTH` | 60 degrees | Length of the visible arc |

### Basic Usage

```cpp
#include <smarthub/ui/widgets/LoadingSpinner.hpp>
#include <smarthub/ui/ThemeManager.hpp>

ThemeManager theme;
LoadingSpinner spinner(parent, theme);

// Show spinner (starts animation)
spinner.show();

// ... perform async operation ...

// Hide spinner (stops animation)
spinner.hide();
```

### Custom Size

```cpp
// Create a larger spinner (64px)
LoadingSpinner largeSpinner(parent, theme, 64);

// Or change size after creation
spinner.setSize(32);  // Make it smaller
```

### Custom Speed

```cpp
// Fast spinner (500ms per rotation)
spinner.setSpeed(500);

// Slow spinner (2 seconds per rotation)
spinner.setSpeed(2000);
```

### Positioning

```cpp
// Center spinner on a specific point
spinner.setPosition(400, 240);  // Center of 800x480 screen
```

### Integration Example

```cpp
void WifiSetupScreen::onConnectClicked() {
    // Show loading indicator
    m_spinner.show();
    m_connectBtn->setEnabled(false);

    // Start async connection
    m_networkManager.connectAsync(ssid, password, [this](bool success) {
        // Hide spinner when done
        m_spinner.hide();
        m_connectBtn->setEnabled(true);

        if (success) {
            showSuccess("Connected!");
        } else {
            showError("Connection failed");
        }
    });
}
```

## ThemeManager Animation Styles

The ThemeManager provides pre-built animation styles for common patterns.

### Button Press Animation

```cpp
ThemeManager theme;
lv_obj_t* btn = lv_btn_create(parent);

// Apply animated press effect
theme.applyButtonPressAnimation(btn);
```

This applies:
- Transform pivot at center
- Scale to 95% on press
- Background color darken on press
- Smooth 100ms transition

### Error Style

Apply a red border to indicate an error state:

```cpp
ThemeManager theme;

// Mark input as invalid
theme.applyErrorStyle(inputField);

// In high contrast mode, also tints background red
```

### Success Style

Apply a green border to indicate success:

```cpp
ThemeManager theme;

// Mark field as valid
theme.applySuccessStyle(inputField);

// In high contrast mode, also tints background green
```

## High Contrast Mode

For accessibility, use high contrast mode:

```cpp
ThemeManager theme;
theme.setMode(ThemeMode::HighContrast);

// Check if high contrast is active
if (theme.isHighContrast()) {
    // Use larger text, bolder borders, etc.
}
```

High contrast colors:
| Color | Hex | Purpose |
|-------|-----|---------|
| Background | #000000 | Pure black |
| Text | #FFFFFF | Pure white |
| Primary | #00FFFF | Cyan (high visibility) |
| Error | #FF0000 | Pure red |
| Success | #00FF00 | Pure green |
| Warning | #FFFF00 | Yellow |

## Best Practices

### 1. Use Appropriate Durations

```cpp
// Quick feedback for direct actions
anim.fadeIn(tooltip, AnimationManager::DURATION_FAST);

// Standard for screen transitions
screenManager.showScreen("settings", TransitionType::SlideLeft);  // Uses 300ms

// Slow for deliberate state changes
anim.animateValue(progressBar, 0, 100, AnimationManager::DURATION_SLOW);
```

### 2. Combine Animations Thoughtfully

```cpp
// Good: Sequential feedback
void onSubmit() {
    if (isValid()) {
        theme.applySuccessStyle(form);
        anim.pulse(submitBtn);
    } else {
        theme.applyErrorStyle(invalidField);
        anim.shake(invalidField);
    }
}
```

### 3. Don't Over-Animate

- Use animations for feedback, not decoration
- Keep durations short (< 500ms for most cases)
- Avoid simultaneous competing animations
- Test on target hardware for performance

### 4. Consider Accessibility

```cpp
// Respect user preferences
if (theme.isHighContrast()) {
    // May want to reduce motion or increase visibility
    anim.fadeIn(obj, AnimationManager::DURATION_FAST);  // Faster
}
```

### 5. Loading States

Always show loading feedback for operations > 200ms:

```cpp
void performAsyncOperation() {
    spinner.show();

    asyncTask([this]() {
        spinner.hide();
        // Show result
    });
}
```

## File Structure

```
app/
├── include/smarthub/ui/
│   ├── AnimationManager.hpp
│   └── widgets/
│       └── LoadingSpinner.hpp
└── src/ui/
    ├── AnimationManager.cpp
    └── widgets/
        └── LoadingSpinner.cpp
```

## Testing

Non-LVGL tests verify constants and construction:

```cpp
// Animation constants
EXPECT_EQ(AnimationManager::DURATION_FAST, 150u);
EXPECT_EQ(AnimationManager::DURATION_NORMAL, 300u);
EXPECT_EQ(AnimationManager::DURATION_SLOW, 500u);
EXPECT_EQ(AnimationManager::PRESS_SCALE, 95);
EXPECT_EQ(AnimationManager::NORMAL_SCALE, 100);

// Spinner constants
EXPECT_EQ(LoadingSpinner::DEFAULT_SIZE, 48);
EXPECT_EQ(LoadingSpinner::DEFAULT_DURATION, 1000u);
EXPECT_EQ(LoadingSpinner::ARC_LENGTH, 60);
```

LVGL-dependent functionality requires hardware testing on the STM32MP157F-DK2.
