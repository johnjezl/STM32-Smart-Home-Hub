/**
 * SmartHub UI Manager Implementation
 *
 * Uses Linux DRM (Direct Rendering Manager) for display output
 * with double buffering and page flipping for smooth rendering.
 */

#include "smarthub/ui/UIManager.hpp"
#include "smarthub/core/EventBus.hpp"
#include "smarthub/core/Logger.hpp"
#include "smarthub/devices/DeviceManager.hpp"

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

// Evdev touch input state
static struct {
    int fd = -1;
    int x = 0;
    int y = 0;
    bool pressed = false;
} s_touch;

static lv_indev_drv_t s_indev_drv;

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

    // Copy pixels to the back buffer
    int32_t x, y;
    for (y = area->y1; y <= area->y2 && y < (int32_t)buf->height; y++) {
        uint32_t* dest = (uint32_t*)(buf->map + y * buf->stride + area->x1 * 4);
        for (x = area->x1; x <= area->x2 && x < (int32_t)buf->width; x++) {
            *dest++ = lv_color_to32(*color_p++);
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
    }

    lv_disp_flush_ready(drv);
}

// Touch input read callback
static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;

    if (s_touch.fd < 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    // Read available events
    struct input_event ev;
    while (read(s_touch.fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                s_touch.x = ev.value;
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                s_touch.y = ev.value;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            s_touch.pressed = (ev.value != 0);
        }
    }

    data->point.x = s_touch.x;
    data->point.y = s_touch.y;
    data->state = s_touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

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

UIManager::UIManager(EventBus& eventBus, DeviceManager& deviceManager)
    : m_eventBus(eventBus)
    , m_deviceManager(deviceManager)
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
    m_width = s_drm.mode.hdisplay;
    m_height = s_drm.mode.vdisplay;

    LOG_INFO("UIManager", "Display: " + std::to_string(m_width) + "x" + std::to_string(m_height) +
             " @ " + std::to_string(s_drm.mode.vrefresh) + "Hz (" + s_drm.mode.name + ")");

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

    // Create double buffers
    for (int i = 0; i < 2; i++) {
        if (!drm_create_buffer(s_drm.fd, &s_drm.buffers[i], m_width, m_height)) {
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

    // Initialize LVGL
    lv_init();

    // Allocate LVGL draw buffers
    size_t buf_size = m_width * m_height / 10;  // 1/10 of screen
    s_lvgl_buf1 = new lv_color_t[buf_size];
    s_lvgl_buf2 = new lv_color_t[buf_size];

    lv_disp_draw_buf_init(&s_draw_buf, s_lvgl_buf1, s_lvgl_buf2, buf_size);

    // Initialize display driver
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = m_width;
    s_disp_drv.ver_res = m_height;
    s_disp_drv.flush_cb = drm_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp_drv.full_refresh = 0;  // Partial updates work with DRM
    lv_disp_drv_register(&s_disp_drv);

    // Initialize input driver
    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    // Create the main screen
    createMainScreen();

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

    // Free LVGL buffers
    delete[] s_lvgl_buf1;
    delete[] s_lvgl_buf2;
    s_lvgl_buf1 = nullptr;
    s_lvgl_buf2 = nullptr;

    m_initialized = false;
    LOG_INFO("UIManager", "UI stopped");
}

void UIManager::createMainScreen() {
    // Get the active screen
    lv_obj_t* scr = lv_scr_act();

    // Set background color
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), LV_PART_MAIN);

    // Create a header bar
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x16213e), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);

    // Header title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "SmartHub");
    lv_obj_set_style_text_color(title, lv_color_hex(0xe94560), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(title);

    // Create a container for the main content
    lv_obj_t* cont = lv_obj_create(scr);
    lv_obj_set_size(cont, LV_PCT(90), LV_PCT(70));
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0f3460), LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);

    // Welcome message
    lv_obj_t* welcome = lv_label_create(cont);
    lv_label_set_text(welcome, "Welcome to SmartHub!\n\nTouch screen to interact");
    lv_obj_set_style_text_color(welcome, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(welcome, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(welcome, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(welcome);

    // Status label at bottom
    lv_obj_t* status = lv_label_create(scr);
    lv_label_set_text(status, "LVGL 8.3.11 | DRM Backend");
    lv_obj_set_style_text_color(status, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -10);
}

} // namespace smarthub
