/**
 * About Screen
 *
 * Displays system information: version, build info, network status,
 * memory usage, and uptime.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <string>

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * About Screen
 *
 * Layout (800x480):
 * +---------------------------------------------------------------+
 * |  < About                                                      |
 * +---------------------------------------------------------------+
 * |  +----------------------------------------------------------+ |
 * |  | SmartHub                                                  | |
 * |  | Version 1.0.0                                             | |
 * |  | Build: 2024-01-15                                         | |
 * |  +----------------------------------------------------------+ |
 * |  | System                                                    | |
 * |  | Platform: STM32MP157F-DK2                                 | |
 * |  | Kernel: Linux 5.15.x                                      | |
 * |  | Uptime: 2d 5h 30m                                         | |
 * |  +----------------------------------------------------------+ |
 * |  | Network                                                   | |
 * |  | IP: 192.168.1.100                                         | |
 * |  | MAC: XX:XX:XX:XX:XX:XX                                    | |
 * |  +----------------------------------------------------------+ |
 * |  | Memory                                                    | |
 * |  | Used: 256 MB / 512 MB (50%)                               | |
 * |  | [=========================-------]                        | |
 * |  +----------------------------------------------------------+ |
 * +---------------------------------------------------------------+
 */
class AboutScreen : public Screen {
public:
    AboutScreen(ScreenManager& screenManager, ThemeManager& theme);
    ~AboutScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

    // System info structure
    struct SystemInfo {
        std::string version = "1.0.0";
        std::string buildDate;
        std::string platform = "STM32MP157F-DK2";
        std::string kernel;
        std::string uptime;
        std::string ipAddress;
        std::string macAddress;
        int memoryUsedMB = 0;
        int memoryTotalMB = 0;
    };

    // For testing
    SystemInfo getSystemInfo() const { return m_systemInfo; }

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createVersionSection();
    void createSystemSection();
    void createNetworkSection();
    void createMemorySection();

    void updateSystemInfo();
    void updateDisplayedInfo();

    void onBackClicked();

    static void backButtonHandler(lv_event_t* e);

    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_content = nullptr;

    // Version section
    lv_obj_t* m_versionLabel = nullptr;
    lv_obj_t* m_buildLabel = nullptr;

    // System section
    lv_obj_t* m_platformLabel = nullptr;
    lv_obj_t* m_kernelLabel = nullptr;
    lv_obj_t* m_uptimeLabel = nullptr;

    // Network section
    lv_obj_t* m_ipLabel = nullptr;
    lv_obj_t* m_macLabel = nullptr;

    // Memory section
    lv_obj_t* m_memoryLabel = nullptr;
    lv_obj_t* m_memoryBar = nullptr;
#endif

    ThemeManager& m_theme;
    SystemInfo m_systemInfo;

    // Helpers for reading system info
    std::string readKernelVersion();
    std::string readUptime();
    std::string readIpAddress();
    std::string readMacAddress();
    void readMemoryInfo(int& usedMB, int& totalMB);
    std::string formatUptime(long seconds);
};

} // namespace ui
} // namespace smarthub
