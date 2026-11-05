#include "headless_display.hpp"

#include <gtest/gtest.h>
#include <lvgl.h>

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

namespace smarthub::ui::test {

namespace {

std::vector<lv_color_t> s_frame(DISPLAY_WIDTH * DISPLAY_HEIGHT);
std::vector<lv_color_t> s_drawBuffer(DISPLAY_WIDTH * DISPLAY_HEIGHT / 10);
lv_disp_draw_buf_t s_drawBufDesc;
lv_disp_drv_t s_driver;
uint32_t s_flushCount = 0;

void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* pixels) {
    const int width = lv_area_get_width(area);
    for (int y = area->y1; y <= area->y2; ++y) {
        std::copy_n(pixels, width, s_frame.begin() + y * DISPLAY_WIDTH + area->x1);
        pixels += width;
    }
    ++s_flushCount;
    lv_disp_flush_ready(driver);
}

// A test can end mid-transition with its screens already deleted. Clear what
// LVGL still holds so the next test starts clean.
class ResetLvglBetweenTests : public ::testing::EmptyTestEventListener {
public:
    void OnTestEnd(const ::testing::TestInfo&) override {
        lv_anim_del_all();

        lv_disp_t* display = lv_disp_get_default();
        display->scr_to_load = nullptr;
        display->prev_scr = nullptr;
        display->del_prev = 0;

        // Set directly: lv_scr_load can't run once the active screen is deleted.
        lv_obj_t* blank = lv_obj_create(nullptr);
        display->act_scr = blank;
        for (uint32_t i = display->screen_cnt; i-- > 0;) {
            lv_obj_t* screen = display->screens[i];
            if (screen != blank && screen != display->top_layer && screen != display->sys_layer) {
                lv_obj_del(screen);
            }
        }
        lv_timer_handler();
    }
};

class HeadlessDisplayEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        lv_init();
        lv_disp_draw_buf_init(&s_drawBufDesc, s_drawBuffer.data(), nullptr,
                              static_cast<uint32_t>(s_drawBuffer.size()));
        lv_disp_drv_init(&s_driver);
        s_driver.hor_res = DISPLAY_WIDTH;
        s_driver.ver_res = DISPLAY_HEIGHT;
        s_driver.flush_cb = flush;
        s_driver.draw_buf = &s_drawBufDesc;
        lv_disp_drv_register(&s_driver);

        ::testing::UnitTest::GetInstance()->listeners().Append(new ResetLvglBetweenTests);
    }
};

[[maybe_unused]] const auto* const s_environment =
    ::testing::AddGlobalTestEnvironment(new HeadlessDisplayEnvironment);

} // namespace

void render() {
    // Enough simulated time for LVGL's default refresh period to elapse.
    for (int i = 0; i < 5; ++i) {
        lv_tick_inc(LV_DISP_DEF_REFR_PERIOD);
        lv_timer_handler();
    }
}

bool transitionPending() {
    return lv_disp_get_default()->scr_to_load != nullptr;
}

void finishTransitions() {
    for (int i = 0; i < 200 && transitionPending(); ++i) {
        lv_tick_inc(LV_DISP_DEF_REFR_PERIOD);
        lv_timer_handler();
    }
}

uint32_t flushCount() {
    return s_flushCount;
}

int countLabels(lv_obj_t* root, const std::string& text) {
    int count = 0;
    if (lv_obj_check_type(root, &lv_label_class) && text == lv_label_get_text(root)) {
        ++count;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(root); ++i) {
        count += countLabels(lv_obj_get_child(root, static_cast<int32_t>(i)), text);
    }
    return count;
}

size_t frameColorCount() {
    std::set<uint32_t> colors;
    for (const lv_color_t& pixel : s_frame) {
        colors.insert(pixel.ch.red << 16 | pixel.ch.green << 8 | pixel.ch.blue);
    }
    return colors.size();
}

bool saveFrame(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;

    std::fprintf(file, "P6\n%d %d\n255\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    for (const lv_color_t& pixel : s_frame) {
        const uint8_t rgb[3] = {pixel.ch.red, pixel.ch.green, pixel.ch.blue};
        std::fwrite(rgb, 1, sizeof(rgb), file);
    }
    return std::fclose(file) == 0;
}

} // namespace smarthub::ui::test
