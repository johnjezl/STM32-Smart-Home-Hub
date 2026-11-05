/**
 * In-memory LVGL display for UI tests.
 *
 * Production sets up LVGL inside UIManager::initialize(), which needs a DRM
 * device. Tests get an 800x480 display that renders to a buffer instead, so
 * screens run against real LVGL with no hardware.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct _lv_obj_t;

namespace smarthub::ui::test {

constexpr int DISPLAY_WIDTH = 800;
constexpr int DISPLAY_HEIGHT = 480;

/** Advance LVGL time and run its timers until the display has been redrawn. */
void render();

/** True while LVGL has a screen load in progress. */
bool transitionPending();

/** Run LVGL until any screen load in progress is finished. */
void finishTransitions();

/** Number of times LVGL has flushed pixels since startup. */
uint32_t flushCount();

/** Number of objects under root (inclusive) that are labels with this exact text. */
int countLabels(_lv_obj_t* root, const std::string& text);

/** Number of distinct colours in the last rendered frame. A blank screen has very few. */
size_t frameColorCount();

/** Save the last rendered frame as a binary PPM image. */
bool saveFrame(const std::string& path);

} // namespace smarthub::ui::test
