/**
 * SmartHub UI Manager Implementation
 *
 * Uses Linux DRM (Direct Rendering Manager) for display output
 * with double buffering and page flipping for smooth rendering.
 */

#include "smarthub/ui/UIManager.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/screens/DashboardScreen.hpp"
#include "smarthub/ui/screens/DeviceListScreen.hpp"
#include "smarthub/ui/screens/SensorListScreen.hpp"
#include "smarthub/ui/screens/SettingsScreen.hpp"
#include "smarthub/ui/screens/LightControlScreen.hpp"
#include "smarthub/ui/screens/RoomDetailScreen.hpp"
#include "smarthub/ui/screens/SecuritySettingsScreen.hpp"
#include "smarthub/ui/screens/AboutScreen.hpp"
#include "smarthub/ui/screens/AddDeviceScreen.hpp"
#include "smarthub/ui/screens/EditDeviceScreen.hpp"
#include "smarthub/ui/screens/WifiSetupScreen.hpp"
#include "smarthub/ui/screens/NotificationScreen.hpp"
#include "smarthub/ui/screens/AutomationListScreen.hpp"
#include "smarthub/ui/screens/AddAutomationScreen.hpp"
#include "smarthub/ui/screens/EditAutomationScreen.hpp"
#include "smarthub/network/NetworkManager.hpp"
#include "smarthub/automation/AutomationManager.hpp"
#include "smarthub/core/EventBus.hpp"
#include "smarthub/core/Logger.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/database/Database.hpp"

#include <lvgl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <vector>

namespace smarthub {

// DRM buffer structure
struct DrmBuffer {
    uint32_t handle = 0;
    uint32_t fb_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t size = 0;
    uint8_t* map = nullptr;
};

// Static DRM state
static struct {
    int fd = -1;
    uint32_t connector_id = 0;
    uint32_t crtc_id = 0;
    drmModeModeInfo mode = {};
    DrmBuffer buffers[2];
    int front_buffer = 0;
    bool page_flip_pending = false;
    drmModeCrtc* saved_crtc = nullptr;
} s_drm;

// LVGL display driver handles
static lv_disp_drv_t s_disp_drv;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t* s_lvgl_buf1 = nullptr;
static lv_color_t* s_lvgl_buf2 = nullptr;

// Touch release debounce time (ms) - prevents spurious release events
static constexpr uint32_t TOUCH_RELEASE_DEBOUNCE_MS = 50;

// Evdev touch input state
static struct {
    int fd = -1;
    int x = -1;  // Raw physical X from touchscreen (-1 = no valid reading)
    int y = -1;  // Raw physical Y from touchscreen (-1 = no valid reading)
    bool pressed = false;
    bool got_coords = false;  // True if we received coordinate events this poll
    // Physical panel dimensions for coordinate transformation
    int phys_width = 0;
    int phys_height = 0;
    // Logical display dimensions (after rotation)
    int logical_width = 0;
    int logical_height = 0;
    bool rotated = false;  // True if display is rotated 90° (portrait->landscape)
    // Debouncing to prevent spurious release events
    uint32_t last_event_time = 0;  // Time of last touch event (ms)
} s_touch;

static lv_indev_drv_t s_indev_drv;

// Keyboard input state
static struct {
    int fd = -1;
    uint32_t last_key = 0;
    lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    bool shift_pressed = false;
    bool caps_lock = false;
} s_keyboard;

static lv_indev_drv_t s_keyboard_drv;
static lv_indev_t* s_keyboard_indev = nullptr;

// Modal focus group stack (for nested modals)
static std::vector<lv_group_t*> s_modal_group_stack;
static lv_group_t* s_main_group = nullptr;

// Forward declarations
static bool drm_create_buffer(int fd, DrmBuffer* buf, uint32_t width, uint32_t height);
static void drm_destroy_buffer(int fd, DrmBuffer* buf);

// Page flip handler callback
static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec,
                               unsigned int tv_usec, void* user_data) {
    (void)fd;
    (void)sequence;
    (void)tv_sec;
    (void)tv_usec;
    (void)user_data;
    s_drm.page_flip_pending = false;
}

// Display rotation state
static bool s_display_rotated = false;
static int s_logical_width = 0;
static int s_logical_height = 0;

// LVGL flush callback - copies rendered pixels to DRM buffer and flips
static void drm_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    if (s_drm.fd < 0) {
        lv_disp_flush_ready(drv);
        return;
    }

    // Get the back buffer
    int back = (s_drm.front_buffer + 1) % 2;
    DrmBuffer* buf = &s_drm.buffers[back];

    if (!buf->map) {
        lv_disp_flush_ready(drv);
        return;
    }

    // Copy pixels to the back buffer with optional rotation
    int32_t lx, ly;  // Logical coordinates (what LVGL renders)

    if (s_display_rotated) {
        // Manual 90° rotation: logical (800x480) -> physical (480x800)
        // physical_x = logical_height - 1 - logical_y
        // physical_y = logical_x
        for (ly = area->y1; ly <= area->y2; ly++) {
            for (lx = area->x1; lx <= area->x2; lx++) {
                int32_t px = s_logical_height - 1 - ly;
                int32_t py = lx;

                if (px >= 0 && px < (int32_t)buf->width && py >= 0 && py < (int32_t)buf->height) {
                    uint32_t* dest = (uint32_t*)(buf->map + py * buf->stride + px * 4);
                    *dest = lv_color_to32(*color_p);
                }
                color_p++;
            }
        }
    } else {
        // No rotation - direct copy
        for (ly = area->y1; ly <= area->y2 && ly < (int32_t)buf->height; ly++) {
            uint32_t* dest = (uint32_t*)(buf->map + ly * buf->stride + area->x1 * 4);
            for (lx = area->x1; lx <= area->x2 && lx < (int32_t)buf->width; lx++) {
                *dest++ = lv_color_to32(*color_p++);
            }
        }
    }

    // If this is the last flush for this frame, do a page flip
    if (lv_disp_flush_is_last(drv)) {
        // Wait for any pending page flip
        while (s_drm.page_flip_pending) {
            drmEventContext ev = {};
            ev.version = 2;
            ev.page_flip_handler = page_flip_handler;
            drmHandleEvent(s_drm.fd, &ev);
        }

        // Request page flip
        int ret = drmModePageFlip(s_drm.fd, s_drm.crtc_id, buf->fb_id,
                                   DRM_MODE_PAGE_FLIP_EVENT, nullptr);
        if (ret == 0) {
            s_drm.page_flip_pending = true;
            s_drm.front_buffer = back;
        }

        // Also copy to the other buffer to keep both in sync
        // This prevents flashing when the buffers have different content
        DrmBuffer* other = &s_drm.buffers[s_drm.front_buffer];
        if (other->map && buf->map && other != buf) {
            memcpy(other->map, buf->map, buf->height * buf->stride);
        }
    }

    lv_disp_flush_ready(drv);
}

// Multi-touch tracking ID for touch state
#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID 0x39
#endif

// Touch input read callback
static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;

    if (s_touch.fd < 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint32_t now = lv_tick_get();

    // Track if we got any events this poll
    bool got_events = false;
    bool explicit_release = false;

    // Read available events
    struct input_event ev;
    while (read(s_touch.fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                s_touch.x = ev.value;
                got_events = true;
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                s_touch.y = ev.value;
                got_events = true;
            } else if (ev.code == ABS_MT_TRACKING_ID) {
                // MT protocol B: tracking_id >= 0 means touch, -1 means release
                if (ev.value >= 0) {
                    s_touch.pressed = true;
                    got_events = true;
                } else {
                    explicit_release = true;
                }
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value != 0) {
                s_touch.pressed = true;
                got_events = true;
            } else {
                explicit_release = true;
            }
        }
    }

    // Update state with debouncing
    if (got_events) {
        // We received touch events - definitely pressed
        s_touch.got_coords = true;
        s_touch.pressed = true;
        s_touch.last_event_time = now;
    } else if (explicit_release) {
        // Explicit release event (BTN_TOUCH=0 or MT_TRACKING_ID=-1)
        // Apply debounce: only release if some time has passed since last event
        if (now - s_touch.last_event_time >= TOUCH_RELEASE_DEBOUNCE_MS) {
            s_touch.pressed = false;
        }
        // If within debounce window, keep pressed state
    } else if (s_touch.pressed && s_touch.got_coords) {
        // No events read, but we were pressed - check debounce timeout
        // Some touchscreens just stop sending events when released
        if (now - s_touch.last_event_time >= TOUCH_RELEASE_DEBOUNCE_MS) {
            s_touch.pressed = false;
        }
    }

    // If no valid coordinates yet, report (0,0) as released
    if (s_touch.x < 0 || s_touch.y < 0) {
        data->point.x = 0;
        data->point.y = 0;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    int lx, ly;

    // Transform touch coordinates for 90° rotation (portrait panel -> landscape display)
    // Display rotation: px = logical_height - 1 - ly, py = lx
    // Inverse (touch): lx = py (touch physical y), ly = logical_height - 1 - px (inverted touch physical x)
    // Physical panel: 480 wide x 800 tall (portrait)
    // Logical display: 800 wide x 480 tall (landscape)
    if (s_touch.rotated) {
        lx = s_touch.y;  // Touch physical Y becomes logical X
        ly = s_touch.logical_height - 1 - s_touch.x;  // Touch physical X inverted becomes logical Y
    } else {
        lx = s_touch.x;
        ly = s_touch.y;
    }

    // Clamp to valid logical display range
    if (lx < 0) lx = 0;
    if (ly < 0) ly = 0;
    if (lx >= s_touch.logical_width) lx = s_touch.logical_width - 1;
    if (ly >= s_touch.logical_height) ly = s_touch.logical_height - 1;

    data->point.x = lx;
    data->point.y = ly;
    data->state = s_touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Map Linux key codes to LVGL key codes
static uint32_t linux_key_to_lvgl(uint16_t code, bool shift, bool caps) {
    switch (code) {
        case KEY_UP:        return LV_KEY_UP;
        case KEY_DOWN:      return LV_KEY_DOWN;
        case KEY_LEFT:      return LV_KEY_LEFT;
        case KEY_RIGHT:     return LV_KEY_RIGHT;
        case KEY_ENTER:     return LV_KEY_ENTER;
        case KEY_ESC:       return LV_KEY_ESC;
        case KEY_BACKSPACE: return LV_KEY_BACKSPACE;
        case KEY_DELETE:    return LV_KEY_DEL;
        case KEY_HOME:      return LV_KEY_HOME;
        case KEY_END:       return LV_KEY_END;
        case KEY_TAB:       return LV_KEY_NEXT;
        default:
            break;
    }

    // For letter keys, caps lock XOR shift determines case
    bool upper = caps != shift;  // XOR: caps+shift = lower, caps or shift alone = upper

    // Number row with shift symbols
    if (code >= KEY_1 && code <= KEY_0) {
        if (shift) {
            static const char shifted_nums[] = "!@#$%^&*()";
            if (code == KEY_0) return ')';
            return shifted_nums[code - KEY_1];
        }
        if (code == KEY_0) return '0';
        return '1' + (code - KEY_1);
    }

    // Letter keys
    if (code >= KEY_Q && code <= KEY_P) {
        static const char top_row[] = "qwertyuiop";
        char c = top_row[code - KEY_Q];
        return upper ? (c - 'a' + 'A') : c;
    }
    if (code >= KEY_A && code <= KEY_L) {
        static const char mid_row[] = "asdfghjkl";
        char c = mid_row[code - KEY_A];
        return upper ? (c - 'a' + 'A') : c;
    }
    if (code >= KEY_Z && code <= KEY_M) {
        static const char bot_row[] = "zxcvbnm";
        char c = bot_row[code - KEY_Z];
        return upper ? (c - 'a' + 'A') : c;
    }

    // Punctuation with shift variants
    if (code == KEY_SPACE) return ' ';
    if (code == KEY_MINUS) return shift ? '_' : '-';
    if (code == KEY_EQUAL) return shift ? '+' : '=';
    if (code == KEY_LEFTBRACE) return shift ? '{' : '[';
    if (code == KEY_RIGHTBRACE) return shift ? '}' : ']';
    if (code == KEY_SEMICOLON) return shift ? ':' : ';';
    if (code == KEY_APOSTROPHE) return shift ? '"' : '\'';
    if (code == KEY_COMMA) return shift ? '<' : ',';
    if (code == KEY_DOT) return shift ? '>' : '.';
    if (code == KEY_SLASH) return shift ? '?' : '/';
    if (code == KEY_BACKSLASH) return shift ? '|' : '\\';
    if (code == KEY_GRAVE) return shift ? '~' : '`';

    return 0;
}

// Keyboard input read callback
static void keyboard_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;

    if (s_keyboard.fd < 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    struct input_event ev;
    while (read(s_keyboard.fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_KEY) {
            // Track modifier keys
            if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                s_keyboard.shift_pressed = (ev.value != 0);
                continue;
            }
            if (ev.code == KEY_CAPSLOCK && ev.value == 1) {
                // Toggle caps lock on key press (not release)
                s_keyboard.caps_lock = !s_keyboard.caps_lock;
                continue;
            }

            // Convert regular keys with shift/caps state
            uint32_t lvgl_key = linux_key_to_lvgl(ev.code, s_keyboard.shift_pressed, s_keyboard.caps_lock);
            if (lvgl_key != 0) {
                s_keyboard.last_key = lvgl_key;
                s_keyboard.state = (ev.value != 0) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            }
        }
    }

    data->key = s_keyboard.last_key;
    data->state = s_keyboard.state;
}

} // namespace smarthub (close temporarily for global functions)

namespace smarthub::ui {

/**
 * Push a new focus group for a modal dialog.
 * Creates a new group, sets it as active for keyboard input, and returns it.
 * Add the modal's interactive widgets to this group.
 * Call popModalFocusGroup() when the modal is closed.
 */
lv_group_t* pushModalFocusGroup() {
    if (!smarthub::s_keyboard_indev) {
        return nullptr;  // No keyboard, nothing to do
    }

    lv_group_t* modal_group = lv_group_create();
    smarthub::s_modal_group_stack.push_back(modal_group);
    lv_indev_set_group(smarthub::s_keyboard_indev, modal_group);

    return modal_group;
}

/**
 * Pop the current modal focus group and restore the previous one.
 * Call this when closing a modal dialog.
 */
void popModalFocusGroup() {
    if (!smarthub::s_keyboard_indev) {
        return;  // No keyboard, nothing to do
    }

    if (smarthub::s_modal_group_stack.empty()) {
        return;  // No modal group to pop
    }

    // Get and remove the current modal group
    lv_group_t* modal_group = smarthub::s_modal_group_stack.back();
    smarthub::s_modal_group_stack.pop_back();

    // Delete the modal group
    lv_group_del(modal_group);

    // Restore the previous group (either another modal or the main group)
    if (!smarthub::s_modal_group_stack.empty()) {
        lv_indev_set_group(smarthub::s_keyboard_indev, smarthub::s_modal_group_stack.back());
    } else {
        lv_indev_set_group(smarthub::s_keyboard_indev, smarthub::s_main_group);
    }
}

} // namespace smarthub::ui

namespace smarthub {

// Create a DRM dumb buffer
static bool drm_create_buffer(int fd, DrmBuffer* buf, uint32_t width, uint32_t height) {
    // Create dumb buffer
    struct drm_mode_create_dumb create = {};
    create.width = width;
    create.height = height;
    create.bpp = 32;

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        LOG_ERROR("UIManager", "Failed to create dumb buffer: " + std::string(strerror(errno)));
        return false;
    }

    buf->handle = create.handle;
    buf->stride = create.pitch;
    buf->size = create.size;
    buf->width = width;
    buf->height = height;

    // Create framebuffer object
    if (drmModeAddFB(fd, width, height, 24, 32, buf->stride, buf->handle, &buf->fb_id) != 0) {
        LOG_ERROR("UIManager", "Failed to create framebuffer: " + std::string(strerror(errno)));
        struct drm_mode_destroy_dumb destroy = { .handle = buf->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        return false;
    }

    // Map buffer to userspace
    struct drm_mode_map_dumb map = {};
    map.handle = buf->handle;

    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        LOG_ERROR("UIManager", "Failed to map dumb buffer: " + std::string(strerror(errno)));
        drmModeRmFB(fd, buf->fb_id);
        struct drm_mode_destroy_dumb destroy = { .handle = buf->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        return false;
    }

    buf->map = (uint8_t*)mmap(nullptr, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
    if (buf->map == MAP_FAILED) {
        LOG_ERROR("UIManager", "Failed to mmap buffer: " + std::string(strerror(errno)));
        drmModeRmFB(fd, buf->fb_id);
        struct drm_mode_destroy_dumb destroy = { .handle = buf->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        buf->map = nullptr;
        return false;
    }

    // Clear buffer to black
    memset(buf->map, 0, buf->size);

    return true;
}

// Destroy a DRM buffer
static void drm_destroy_buffer(int fd, DrmBuffer* buf) {
    if (buf->map) {
        munmap(buf->map, buf->size);
        buf->map = nullptr;
    }

    if (buf->fb_id) {
        drmModeRmFB(fd, buf->fb_id);
        buf->fb_id = 0;
    }

    if (buf->handle) {
        struct drm_mode_destroy_dumb destroy = { .handle = buf->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        buf->handle = 0;
    }
}

UIManager::UIManager(EventBus& eventBus, DeviceManager& deviceManager, Database& database)
    : m_eventBus(eventBus)
    , m_deviceManager(deviceManager)
    , m_database(database)
{
}

UIManager::~UIManager() {
    shutdown();
}

bool UIManager::initialize(const std::string& drmDevice, const std::string& touchDevice) {
    m_fbDevice = drmDevice;  // Reuse member name for DRM device path
    m_touchDevice = touchDevice;

    LOG_INFO("UIManager", "Initializing LVGL UI with DRM backend");

    // Open DRM device
    s_drm.fd = open(drmDevice.c_str(), O_RDWR | O_CLOEXEC);
    if (s_drm.fd < 0) {
        LOG_ERROR("UIManager", "Failed to open DRM device: " + drmDevice + " (" + strerror(errno) + ")");
        return false;
    }

    // Get DRM resources
    drmModeRes* res = drmModeGetResources(s_drm.fd);
    if (!res) {
        LOG_ERROR("UIManager", "Failed to get DRM resources");
        close(s_drm.fd);
        s_drm.fd = -1;
        return false;
    }

    // Find a connected connector
    drmModeConnector* connector = nullptr;
    for (int i = 0; i < res->count_connectors; i++) {
        connector = drmModeGetConnector(s_drm.fd, res->connectors[i]);
        if (connector && connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            s_drm.connector_id = connector->connector_id;
            break;
        }
        if (connector) {
            drmModeFreeConnector(connector);
            connector = nullptr;
        }
    }

    if (!connector) {
        LOG_ERROR("UIManager", "No connected display found");
        drmModeFreeResources(res);
        close(s_drm.fd);
        s_drm.fd = -1;
        return false;
    }

    // Use the first (preferred) mode
    s_drm.mode = connector->modes[0];
    int phys_width = s_drm.mode.hdisplay;
    int phys_height = s_drm.mode.vdisplay;

    LOG_INFO("UIManager", "Physical display: " + std::to_string(phys_width) + "x" + std::to_string(phys_height) +
             " @ " + std::to_string(s_drm.mode.vrefresh) + "Hz (" + s_drm.mode.name + ")");

    // Detect portrait mode and enable rotation to landscape
    // Portrait: height > width (e.g., 480x800)
    // Landscape: width > height (e.g., 800x480)
    bool needs_rotation = (phys_height > phys_width);

    if (needs_rotation) {
        // Swap dimensions for landscape orientation
        m_width = phys_height;   // Logical width = physical height
        m_height = phys_width;   // Logical height = physical width
        LOG_INFO("UIManager", "Rotating display 90° to landscape: " +
                 std::to_string(m_width) + "x" + std::to_string(m_height));
    } else {
        m_width = phys_width;
        m_height = phys_height;
    }

    // Store dimensions for touch coordinate transformation
    s_touch.phys_width = phys_width;
    s_touch.phys_height = phys_height;
    s_touch.logical_width = m_width;
    s_touch.logical_height = m_height;
    s_touch.rotated = needs_rotation;
    s_touch.x = -1;  // Reset to indicate no valid touch yet
    s_touch.y = -1;

    // Find encoder and CRTC
    drmModeEncoder* encoder = nullptr;
    if (connector->encoder_id) {
        encoder = drmModeGetEncoder(s_drm.fd, connector->encoder_id);
    }

    if (encoder) {
        s_drm.crtc_id = encoder->crtc_id;
        drmModeFreeEncoder(encoder);
    } else {
        // Find a CRTC that can drive this connector
        for (int i = 0; i < res->count_crtcs; i++) {
            if (connector->encoders) {
                for (int j = 0; j < connector->count_encoders; j++) {
                    drmModeEncoder* enc = drmModeGetEncoder(s_drm.fd, connector->encoders[j]);
                    if (enc && (enc->possible_crtcs & (1 << i))) {
                        s_drm.crtc_id = res->crtcs[i];
                        drmModeFreeEncoder(enc);
                        goto crtc_found;
                    }
                    if (enc) drmModeFreeEncoder(enc);
                }
            }
        }
        crtc_found:;
    }

    if (!s_drm.crtc_id) {
        LOG_ERROR("UIManager", "No CRTC found for connector");
        drmModeFreeConnector(connector);
        drmModeFreeResources(res);
        close(s_drm.fd);
        s_drm.fd = -1;
        return false;
    }

    // Save current CRTC state for restoration on exit
    s_drm.saved_crtc = drmModeGetCrtc(s_drm.fd, s_drm.crtc_id);

    drmModeFreeConnector(connector);
    drmModeFreeResources(res);

    // Create double buffers at physical dimensions (DRM operates at physical resolution)
    for (int i = 0; i < 2; i++) {
        if (!drm_create_buffer(s_drm.fd, &s_drm.buffers[i], phys_width, phys_height)) {
            LOG_ERROR("UIManager", "Failed to create DRM buffer " + std::to_string(i));
            // Cleanup already created buffers
            for (int j = 0; j < i; j++) {
                drm_destroy_buffer(s_drm.fd, &s_drm.buffers[j]);
            }
            if (s_drm.saved_crtc) {
                drmModeFreeCrtc(s_drm.saved_crtc);
                s_drm.saved_crtc = nullptr;
            }
            close(s_drm.fd);
            s_drm.fd = -1;
            return false;
        }
    }

    // Clear both buffers to black before displaying
    for (int i = 0; i < 2; i++) {
        if (s_drm.buffers[i].map) {
            memset(s_drm.buffers[i].map, 0, s_drm.buffers[i].size);
        }
    }

    // Set initial mode
    if (drmModeSetCrtc(s_drm.fd, s_drm.crtc_id, s_drm.buffers[0].fb_id, 0, 0,
                       &s_drm.connector_id, 1, &s_drm.mode) != 0) {
        LOG_ERROR("UIManager", "Failed to set CRTC mode: " + std::string(strerror(errno)));
        for (int i = 0; i < 2; i++) {
            drm_destroy_buffer(s_drm.fd, &s_drm.buffers[i]);
        }
        if (s_drm.saved_crtc) {
            drmModeFreeCrtc(s_drm.saved_crtc);
            s_drm.saved_crtc = nullptr;
        }
        close(s_drm.fd);
        s_drm.fd = -1;
        return false;
    }

    // Open touch device (optional - non-fatal if not available)
    s_touch.fd = open(touchDevice.c_str(), O_RDONLY | O_NONBLOCK);
    if (s_touch.fd < 0) {
        LOG_WARN("UIManager", "Touch device not available: " + touchDevice);
    } else {
        LOG_INFO("UIManager", "Touch input initialized: " + touchDevice);
    }

    // Try to find and open a keyboard device
    // We look for a full keyboard (has KEY_A, KEY_Z, and KEY_ENTER) rather than
    // just KEY_A to avoid opening G-key/macro keypads
    s_keyboard.fd = -1;
    LOG_INFO("Scanning for keyboard devices...");
    for (int i = 0; i < 10 && s_keyboard.fd < 0; i++) {
        std::string kbdPath = "/dev/input/event" + std::to_string(i);
        int fd = open(kbdPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            unsigned long evbit = 0;
            unsigned long keybit[(KEY_MAX + 8 * sizeof(long) - 1) / (8 * sizeof(long))] = {0};

            int ev_ret = ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);
            bool has_ev_key = (evbit & (1UL << EV_KEY)) != 0;
            bool has_ev_led = (evbit & (1UL << EV_LED)) != 0;  // Full keyboards have LEDs
            int key_ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);

            // Check for multiple keys to identify a full keyboard
            auto has_key = [&keybit, key_ret](int key) -> bool {
                if (key_ret < 0) return false;
                size_t idx = key / (8 * sizeof(long));
                unsigned long bit = 1UL << (key % (8 * sizeof(long)));
                return (keybit[idx] & bit) != 0;
            };

            // A real QWERTY keyboard must have most letter keys
            // Consumer Control devices don't have the full alphabet
            int letter_key_count = 0;
            if (has_key(KEY_Q)) letter_key_count++;
            if (has_key(KEY_W)) letter_key_count++;
            if (has_key(KEY_E)) letter_key_count++;
            if (has_key(KEY_R)) letter_key_count++;
            if (has_key(KEY_T)) letter_key_count++;
            if (has_key(KEY_Y)) letter_key_count++;
            if (has_key(KEY_U)) letter_key_count++;
            if (has_key(KEY_I)) letter_key_count++;
            if (has_key(KEY_O)) letter_key_count++;
            if (has_key(KEY_P)) letter_key_count++;
            if (has_key(KEY_A)) letter_key_count++;
            if (has_key(KEY_S)) letter_key_count++;
            if (has_key(KEY_D)) letter_key_count++;
            if (has_key(KEY_F)) letter_key_count++;
            if (has_key(KEY_G)) letter_key_count++;
            if (has_key(KEY_H)) letter_key_count++;
            if (has_key(KEY_J)) letter_key_count++;
            if (has_key(KEY_K)) letter_key_count++;
            if (has_key(KEY_L)) letter_key_count++;
            if (has_key(KEY_Z)) letter_key_count++;
            if (has_key(KEY_X)) letter_key_count++;
            if (has_key(KEY_C)) letter_key_count++;
            if (has_key(KEY_V)) letter_key_count++;
            if (has_key(KEY_B)) letter_key_count++;
            if (has_key(KEY_N)) letter_key_count++;
            if (has_key(KEY_M)) letter_key_count++;

            // Require at least 20 letter keys (full QWERTY has 26)
            bool is_full_keyboard = (letter_key_count >= 20) && has_key(KEY_SPACE) && has_key(KEY_ENTER);

            LOG_INFO("Checking %s: evbit=0x%lx LED=%d letters=%d space=%d enter=%d full=%d",
                      kbdPath.c_str(), evbit, has_ev_led, letter_key_count,
                      has_key(KEY_SPACE), has_key(KEY_ENTER), is_full_keyboard);

            // Require full keyboard to avoid consumer control devices
            if (ev_ret >= 0 && has_ev_key && is_full_keyboard) {
                s_keyboard.fd = fd;
                LOG_INFO("Keyboard found and opened: %s", kbdPath.c_str());
            } else {
                close(fd);
            }
        }
    }
    if (s_keyboard.fd < 0) {
        LOG_WARN("No keyboard device found after scanning");
    }

    // Initialize LVGL
    lv_init();

    // Allocate LVGL draw buffers - full screen size for single-flush rendering
    // This ensures the entire frame is rendered before any page flip occurs,
    // preventing partial-update flashing artifacts
    size_t buf_size = m_width * m_height;  // Full screen
    s_lvgl_buf1 = new lv_color_t[buf_size];
    s_lvgl_buf2 = new lv_color_t[buf_size];

    lv_disp_draw_buf_init(&s_draw_buf, s_lvgl_buf1, s_lvgl_buf2, buf_size);

    // Set rotation state for flush callback
    s_display_rotated = needs_rotation;
    s_logical_width = m_width;
    s_logical_height = m_height;

    // Initialize display driver
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = m_width;   // Logical resolution (800x480 in landscape)
    s_disp_drv.ver_res = m_height;
    s_disp_drv.flush_cb = drm_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    // Always use full refresh with DRM double-buffering to ensure both buffers
    // stay synchronized. Without this, partial updates to one buffer could leave
    // stale content that appears as black regions when buffers are swapped.
    s_disp_drv.full_refresh = 1;
    // Note: We handle rotation manually in drm_flush_cb, not using sw_rotate

    lv_disp_drv_register(&s_disp_drv);

    // Initialize touch input driver
    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    // Initialize keyboard input driver
    if (s_keyboard.fd >= 0) {
        lv_indev_drv_init(&s_keyboard_drv);
        s_keyboard_drv.type = LV_INDEV_TYPE_KEYPAD;
        s_keyboard_drv.read_cb = keyboard_read_cb;
        s_keyboard_indev = lv_indev_drv_register(&s_keyboard_drv);

        // Create a group for keyboard navigation and set it as default
        s_main_group = lv_group_create();
        lv_group_set_default(s_main_group);
        lv_indev_set_group(s_keyboard_indev, s_main_group);
    }

    // Set up screens and show dashboard
    setupScreens();

    // Clear any stale objects from the top layer to prevent overlay issues
    // This ensures no leftover modal dialogs or overlays are visible at startup
    lv_obj_clean(lv_layer_top());

    // Force immediate render to BOTH buffers to ensure double-buffering is properly initialized
    // Without this, one buffer may contain stale/black content causing flickering on first interaction
    // Render 1: Renders to back buffer (buffer 1), then flips (buffer 1 becomes front)
    lv_refr_now(NULL);
    lv_timer_handler();

    // Wait for first page flip to complete
    while (s_drm.page_flip_pending) {
        drmEventContext ev = {};
        ev.version = 2;
        ev.page_flip_handler = page_flip_handler;
        drmHandleEvent(s_drm.fd, &ev);
    }

    // Render 2: Force another full refresh to populate the other buffer
    // This marks the entire screen dirty and renders to the now-back buffer (buffer 0)
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    lv_timer_handler();

    // Wait for second page flip to complete
    while (s_drm.page_flip_pending) {
        drmEventContext ev = {};
        ev.version = 2;
        ev.page_flip_handler = page_flip_handler;
        drmHandleEvent(s_drm.fd, &ev);
    }

    LOG_DEBUG("UIManager", "Both DRM buffers initialized with rendered content");

    m_initialized = true;
    m_running = true;

    // Initialize tick counter
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    m_lastTick = static_cast<uint32_t>(ms.count());

    LOG_INFO("UIManager", "LVGL initialized with DRM backend successfully");

    return true;
}

void UIManager::update() {
    if (!m_initialized || !m_running) {
        return;
    }

    // Update LVGL tick
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    uint32_t currentTick = static_cast<uint32_t>(ms.count());
    uint32_t elapsed = currentTick - m_lastTick;
    m_lastTick = currentTick;

    lv_tick_inc(elapsed);

    // Update screen manager (for screen animations and updates)
    if (m_screenManager) {
        m_screenManager->update(elapsed);
    }

    // Poll automation manager for time-based triggers
    if (m_automationManager) {
        m_automationManager->poll(currentTick);
    }

    // Handle LVGL tasks
    lv_timer_handler();

    // Handle any pending DRM events (page flip completion)
    if (s_drm.page_flip_pending) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(s_drm.fd, &fds);

        struct timeval tv = { 0, 0 };  // Non-blocking
        if (select(s_drm.fd + 1, &fds, nullptr, nullptr, &tv) > 0) {
            drmEventContext ev = {};
            ev.version = 2;
            ev.page_flip_handler = page_flip_handler;
            drmHandleEvent(s_drm.fd, &ev);
        }
    }
}

void UIManager::shutdown() {
    if (m_running) {
        m_running = false;
        LOG_INFO("UIManager", "UI stopping");
    }

    // Wait for pending page flip
    while (s_drm.page_flip_pending && s_drm.fd >= 0) {
        drmEventContext ev = {};
        ev.version = 2;
        ev.page_flip_handler = page_flip_handler;
        drmHandleEvent(s_drm.fd, &ev);
    }

    // Restore saved CRTC
    if (s_drm.saved_crtc && s_drm.fd >= 0) {
        drmModeSetCrtc(s_drm.fd, s_drm.saved_crtc->crtc_id, s_drm.saved_crtc->buffer_id,
                       s_drm.saved_crtc->x, s_drm.saved_crtc->y,
                       &s_drm.connector_id, 1, &s_drm.saved_crtc->mode);
        drmModeFreeCrtc(s_drm.saved_crtc);
        s_drm.saved_crtc = nullptr;
    }

    // Destroy DRM buffers
    for (int i = 0; i < 2; i++) {
        drm_destroy_buffer(s_drm.fd, &s_drm.buffers[i]);
    }

    // Close DRM device
    if (s_drm.fd >= 0) {
        close(s_drm.fd);
        s_drm.fd = -1;
    }

    // Close touch device
    if (s_touch.fd >= 0) {
        close(s_touch.fd);
        s_touch.fd = -1;
    }

    // Close keyboard device
    if (s_keyboard.fd >= 0) {
        close(s_keyboard.fd);
        s_keyboard.fd = -1;
    }

    // Clean up screen manager before LVGL buffers
    m_screenManager.reset();
    m_themeManager.reset();
    m_automationManager.reset();

    // Free LVGL buffers
    delete[] s_lvgl_buf1;
    delete[] s_lvgl_buf2;
    s_lvgl_buf1 = nullptr;
    s_lvgl_buf2 = nullptr;

    m_initialized = false;
    LOG_INFO("UIManager", "UI stopped");
}

void UIManager::setupScreens() {
    // Create theme manager
    m_themeManager = std::make_unique<ui::ThemeManager>();

    // Create network manager for WiFi setup
    m_networkManager = std::make_unique<network::NetworkManager>();
    m_networkManager->initialize();

    // Create automation manager
    m_automationManager = std::make_unique<AutomationManager>(
        m_eventBus, m_database, m_deviceManager);
    m_automationManager->initialize();

    // Create screen manager
    m_screenManager = std::make_unique<ui::ScreenManager>(*this);

    // Register screens that we have dependencies for
    m_screenManager->registerScreen("dashboard",
        std::make_unique<ui::DashboardScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("devices",
        std::make_unique<ui::DeviceListScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("sensors",
        std::make_unique<ui::SensorListScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("settings",
        std::make_unique<ui::SettingsScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("lights",
        std::make_unique<ui::LightControlScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("room_detail",
        std::make_unique<ui::RoomDetailScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("security",
        std::make_unique<ui::SecuritySettingsScreen>(*m_screenManager, *m_themeManager));

    m_screenManager->registerScreen("about",
        std::make_unique<ui::AboutScreen>(*m_screenManager, *m_themeManager));

    m_screenManager->registerScreen("add_device",
        std::make_unique<ui::AddDeviceScreen>(*m_screenManager, *m_themeManager, m_deviceManager, m_eventBus));

    m_screenManager->registerScreen("edit_device",
        std::make_unique<ui::EditDeviceScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("wifi_setup",
        std::make_unique<ui::WifiSetupScreen>(*m_screenManager, *m_themeManager, *m_networkManager));

    m_screenManager->registerScreen("notifications",
        std::make_unique<ui::NotificationScreen>(*m_screenManager, *m_themeManager, m_deviceManager));

    m_screenManager->registerScreen("automations",
        std::make_unique<ui::AutomationListScreen>(*m_screenManager, *m_themeManager, *m_automationManager));

    m_screenManager->registerScreen("add_automation",
        std::make_unique<ui::AddAutomationScreen>(*m_screenManager, *m_themeManager,
            *m_automationManager, m_deviceManager));

    m_screenManager->registerScreen("edit_automation",
        std::make_unique<ui::EditAutomationScreen>(*m_screenManager, *m_themeManager, *m_automationManager));

    // Set home screen and show it
    m_screenManager->setHomeScreen("dashboard");
    m_screenManager->showScreen("dashboard", ui::TransitionType::None, false);

    LOG_INFO("UIManager", "Screens initialized");
}

} // namespace smarthub
