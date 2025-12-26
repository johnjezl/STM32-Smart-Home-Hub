/**
 * SmartHub - Main Application Entry Point
 *
 * STM32MP157F-DK2 Smart Home Hub
 * Runs on Cortex-A7 under Buildroot Linux
 */

#include <cstdio>
#include <cstdlib>
#include <csignal>

// TODO: Include core modules
// #include "core/logger.hpp"
// #include "core/config.hpp"
// #include "core/event_bus.hpp"
// #include "database/database.hpp"
// #include "devices/device_manager.hpp"
// #include "ui/ui_manager.hpp"
// #include "web/web_server.hpp"
// #include "rpmsg/rpmsg_client.hpp"

namespace {
    volatile bool g_running = true;

    void signal_handler(int signum) {
        if (signum == SIGINT || signum == SIGTERM) {
            g_running = false;
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::printf("SmartHub v0.1.0 starting...\n");

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // TODO: Initialize subsystems
    // 1. Load configuration
    // 2. Initialize logging
    // 3. Initialize database
    // 4. Start event bus
    // 5. Initialize device manager
    // 6. Start LVGL UI
    // 7. Start web server
    // 8. Connect to M4 via RPMsg
    // 9. Start MQTT broker connection

    std::printf("SmartHub initialized. Press Ctrl+C to exit.\n");

    // Main loop
    while (g_running) {
        // TODO: Process events
        // - Handle UI updates (LVGL tick)
        // - Process incoming messages
        // - Check device states
    }

    std::printf("SmartHub shutting down...\n");

    // TODO: Cleanup subsystems

    return EXIT_SUCCESS;
}
