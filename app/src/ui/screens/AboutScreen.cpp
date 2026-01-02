/**
 * About Screen Implementation
 */

#include "smarthub/ui/screens/AboutScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/core/Logger.hpp"

#include <fstream>
#include <sstream>
#include <cstdio>
#include <array>
#include <ctime>

namespace smarthub {
namespace ui {

AboutScreen::AboutScreen(ScreenManager& screenManager, ThemeManager& theme)
    : Screen(screenManager, "about")
    , m_theme(theme)
{
    // Set build date at compile time
    m_systemInfo.buildDate = __DATE__;
}

AboutScreen::~AboutScreen() = default;

void AboutScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();

    // Content area
    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), 480 - Header::HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_set_flex_flow(m_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_add_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);

    createVersionSection();
    createSystemSection();
    createNetworkSection();
    createMemorySection();

    LOG_DEBUG("AboutScreen created");
#endif
}

void AboutScreen::onShow() {
    updateSystemInfo();
#ifdef SMARTHUB_ENABLE_LVGL
    updateDisplayedInfo();
#endif
    LOG_DEBUG("AboutScreen shown");
}

void AboutScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
    // Could periodically update uptime/memory if desired
}

void AboutScreen::onDestroy() {
    LOG_DEBUG("AboutScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void AboutScreen::createHeader() {
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    m_backBtn = lv_btn_create(header);
    lv_obj_set_size(m_backBtn, 40, 40);
    lv_obj_align(m_backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_backBtn, 0, 0);
    lv_obj_add_event_cb(m_backBtn, backButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title
    m_titleLabel = lv_label_create(header);
    lv_label_set_text(m_titleLabel, "About");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, 60, 0);
}

void AboutScreen::createVersionSection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 90);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // App name
    lv_obj_t* appName = lv_label_create(section);
    lv_label_set_text(appName, "SmartHub");
    lv_obj_set_style_text_color(appName, m_theme.primary(), 0);
    lv_obj_set_style_text_font(appName, &lv_font_montserrat_20, 0);
    lv_obj_align(appName, LV_ALIGN_TOP_LEFT, 0, 0);

    // Version
    m_versionLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_versionLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_versionLabel, LV_ALIGN_TOP_LEFT, 0, 28);

    // Build date
    m_buildLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_buildLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_buildLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_buildLabel, LV_ALIGN_TOP_LEFT, 0, 50);
}

void AboutScreen::createSystemSection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 110);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // Section title
    lv_obj_t* title = lv_label_create(section);
    lv_label_set_text(title, "System");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Platform
    m_platformLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_platformLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_platformLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_platformLabel, LV_ALIGN_TOP_LEFT, 0, 28);

    // Kernel
    m_kernelLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_kernelLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_kernelLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_kernelLabel, LV_ALIGN_TOP_LEFT, 0, 48);

    // Uptime
    m_uptimeLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_uptimeLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_uptimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_uptimeLabel, LV_ALIGN_TOP_LEFT, 0, 68);
}

void AboutScreen::createNetworkSection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 80);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // Section title
    lv_obj_t* title = lv_label_create(section);
    lv_label_set_text(title, "Network");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // IP Address
    m_ipLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_ipLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_ipLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_ipLabel, LV_ALIGN_TOP_LEFT, 0, 28);

    // MAC Address
    m_macLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_macLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_macLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_macLabel, LV_ALIGN_TOP_LEFT, 0, 48);
}

void AboutScreen::createMemorySection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 80);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // Section title
    lv_obj_t* title = lv_label_create(section);
    lv_label_set_text(title, "Memory");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Memory usage text
    m_memoryLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_memoryLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_memoryLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(m_memoryLabel, LV_ALIGN_TOP_LEFT, 0, 28);

    // Memory bar
    m_memoryBar = lv_bar_create(section);
    lv_obj_set_size(m_memoryBar, lv_pct(100) - 2 * ThemeManager::CARD_PADDING, 12);
    lv_obj_align(m_memoryBar, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_bar_set_range(m_memoryBar, 0, 100);
    lv_obj_set_style_bg_color(m_memoryBar, m_theme.primary(), LV_PART_INDICATOR);
}

void AboutScreen::updateDisplayedInfo() {
    if (m_versionLabel) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Version %s", m_systemInfo.version.c_str());
        lv_label_set_text(m_versionLabel, buf);
    }

    if (m_buildLabel) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Build: %s", m_systemInfo.buildDate.c_str());
        lv_label_set_text(m_buildLabel, buf);
    }

    if (m_platformLabel) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Platform: %s", m_systemInfo.platform.c_str());
        lv_label_set_text(m_platformLabel, buf);
    }

    if (m_kernelLabel) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Kernel: %s", m_systemInfo.kernel.c_str());
        lv_label_set_text(m_kernelLabel, buf);
    }

    if (m_uptimeLabel) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Uptime: %s", m_systemInfo.uptime.c_str());
        lv_label_set_text(m_uptimeLabel, buf);
    }

    if (m_ipLabel) {
        char buf[64];
        snprintf(buf, sizeof(buf), "IP: %s",
                 m_systemInfo.ipAddress.empty() ? "Not connected" : m_systemInfo.ipAddress.c_str());
        lv_label_set_text(m_ipLabel, buf);
    }

    if (m_macLabel) {
        char buf[64];
        snprintf(buf, sizeof(buf), "MAC: %s",
                 m_systemInfo.macAddress.empty() ? "Unknown" : m_systemInfo.macAddress.c_str());
        lv_label_set_text(m_macLabel, buf);
    }

    if (m_memoryLabel && m_memoryBar) {
        char buf[64];
        int percent = 0;
        if (m_systemInfo.memoryTotalMB > 0) {
            percent = (m_systemInfo.memoryUsedMB * 100) / m_systemInfo.memoryTotalMB;
        }
        snprintf(buf, sizeof(buf), "Used: %d MB / %d MB (%d%%)",
                 m_systemInfo.memoryUsedMB, m_systemInfo.memoryTotalMB, percent);
        lv_label_set_text(m_memoryLabel, buf);
        lv_bar_set_value(m_memoryBar, percent, LV_ANIM_OFF);
    }
}

void AboutScreen::onBackClicked() {
    m_screenManager.goBack();
}

void AboutScreen::backButtonHandler(lv_event_t* e) {
    AboutScreen* self = static_cast<AboutScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

#endif // SMARTHUB_ENABLE_LVGL

void AboutScreen::updateSystemInfo() {
    m_systemInfo.kernel = readKernelVersion();
    m_systemInfo.uptime = readUptime();
    m_systemInfo.ipAddress = readIpAddress();
    m_systemInfo.macAddress = readMacAddress();
    readMemoryInfo(m_systemInfo.memoryUsedMB, m_systemInfo.memoryTotalMB);
}

std::string AboutScreen::readKernelVersion() {
    std::ifstream file("/proc/version");
    if (file.is_open()) {
        std::string line;
        std::getline(file, line);
        // Parse "Linux version X.Y.Z ..."
        size_t pos = line.find("Linux version ");
        if (pos != std::string::npos) {
            pos += 14;
            size_t end = line.find(' ', pos);
            if (end != std::string::npos) {
                return "Linux " + line.substr(pos, end - pos);
            }
        }
    }
    return "Unknown";
}

std::string AboutScreen::readUptime() {
    std::ifstream file("/proc/uptime");
    if (file.is_open()) {
        double uptimeSeconds;
        file >> uptimeSeconds;
        return formatUptime(static_cast<long>(uptimeSeconds));
    }
    return "Unknown";
}

std::string AboutScreen::formatUptime(long seconds) {
    int days = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int minutes = (seconds % 3600) / 60;

    std::ostringstream oss;
    if (days > 0) {
        oss << days << "d ";
    }
    if (hours > 0 || days > 0) {
        oss << hours << "h ";
    }
    oss << minutes << "m";
    return oss.str();
}

std::string AboutScreen::readIpAddress() {
    // Try to read from hostname -I or ip addr
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen("hostname -I 2>/dev/null | awk '{print $1}'", "r");
    if (pipe) {
        if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result = buffer.data();
            // Remove trailing newline
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
        }
        pclose(pipe);
    }

    return result.empty() ? "" : result;
}

std::string AboutScreen::readMacAddress() {
    // Try common network interfaces
    const char* interfaces[] = {"eth0", "wlan0", "enp0s3", "end0"};

    for (const char* iface : interfaces) {
        std::string path = std::string("/sys/class/net/") + iface + "/address";
        std::ifstream file(path);
        if (file.is_open()) {
            std::string mac;
            std::getline(file, mac);
            if (!mac.empty() && mac != "00:00:00:00:00:00") {
                return mac;
            }
        }
    }

    return "";
}

void AboutScreen::readMemoryInfo(int& usedMB, int& totalMB) {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        usedMB = 0;
        totalMB = 0;
        return;
    }

    long memTotal = 0;
    long memFree = 0;
    long buffers = 0;
    long cached = 0;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("MemTotal:") == 0) {
            sscanf(line.c_str(), "MemTotal: %ld kB", &memTotal);
        } else if (line.find("MemFree:") == 0) {
            sscanf(line.c_str(), "MemFree: %ld kB", &memFree);
        } else if (line.find("Buffers:") == 0) {
            sscanf(line.c_str(), "Buffers: %ld kB", &buffers);
        } else if (line.find("Cached:") == 0 && line.find("SwapCached:") != 0) {
            sscanf(line.c_str(), "Cached: %ld kB", &cached);
        }
    }

    totalMB = memTotal / 1024;
    // Used = Total - Free - Buffers - Cached (actual used memory)
    long usedKB = memTotal - memFree - buffers - cached;
    usedMB = usedKB / 1024;

    if (usedMB < 0) usedMB = 0;
}

} // namespace ui
} // namespace smarthub
