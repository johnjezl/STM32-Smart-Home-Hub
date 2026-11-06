/**
 * SmartHub Application
 *
 * Main application class that orchestrates all subsystems.
 */
#pragma once

#include <string>
#include <memory>
#include <atomic>

namespace smarthub {

// Forward declarations
class Config;
class EventBus;
class Database;
class DeviceManager;
class WebServer;
class MqttClient;
class RpmsgClient;
class AutomationManager;

namespace security {
class UserManager;
class SessionManager;
class ApiTokenManager;
}

#ifdef SMARTHUB_ENABLE_LVGL
class UIManager;
#endif

/**
 * Main application class
 *
 * Manages initialization, main loop, and shutdown of all
 * SmartHub subsystems.
 */
class Application {
public:
    /**
     * Construct application with config file path
     * @param configPath Path to configuration YAML file
     */
    explicit Application(const std::string& configPath);
    ~Application();

    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * Initialize all subsystems
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * Run the main loop until stopRequested is set, then shut down.
     * Setting the flag is safe from any thread or a signal handler.
     */
    void run(const std::atomic<bool>& stopRequested);

    /**
     * Stop every subsystem. Does nothing if that's already happened. Call it from the thread
     * that called initialize(), and not while run() is running.
     */
    void shutdown();

    // Component accessors
    Config& config();
    EventBus& eventBus();
    Database& database();
    DeviceManager& deviceManager();

    /**
     * Get application version string
     */
    static const char* version() { return "0.1.0"; }

private:
    bool initializeConfig();
    bool initializeDatabase();
    bool initializeDeviceManager();
    bool initializeAutomation();
    bool initializeMqtt();
    bool initializeRpmsg();
    bool initializeSecurity();
    bool initializeWebServer();
    bool initializeUI();

    void mainLoop(const std::atomic<bool>& stopRequested);
    void shutdownSubsystems();

    std::string m_configPath;
    bool m_shutDown = false;
    std::atomic<bool> m_initialized{false};

    // Subsystems (order matters for initialization/shutdown)
    std::unique_ptr<Config> m_config;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<DeviceManager> m_deviceManager;
    std::unique_ptr<AutomationManager> m_automationManager;
    std::unique_ptr<MqttClient> m_mqttClient;
    std::unique_ptr<RpmsgClient> m_rpmsgClient;
    std::unique_ptr<security::UserManager> m_userManager;
    std::unique_ptr<security::SessionManager> m_sessionManager;
    std::unique_ptr<security::ApiTokenManager> m_apiTokenManager;
    std::unique_ptr<WebServer> m_webServer;

#ifdef SMARTHUB_ENABLE_LVGL
    std::unique_ptr<UIManager> m_uiManager;
#endif
};

} // namespace smarthub
