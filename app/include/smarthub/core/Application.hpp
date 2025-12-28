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
     * Run the main application loop
     * Blocks until shutdown() is called
     */
    void run();

    /**
     * Request application shutdown
     * Can be called from signal handler
     */
    void shutdown();

    /**
     * Check if application is running
     */
    bool isRunning() const { return m_running.load(); }

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
    bool initializeMqtt();
    bool initializeRpmsg();
    bool initializeWebServer();
    bool initializeUI();

    void mainLoop();
    void shutdownSubsystems();

    std::string m_configPath;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};

    // Subsystems (order matters for initialization/shutdown)
    std::unique_ptr<Config> m_config;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<DeviceManager> m_deviceManager;
    std::unique_ptr<MqttClient> m_mqttClient;
    std::unique_ptr<RpmsgClient> m_rpmsgClient;
    std::unique_ptr<WebServer> m_webServer;

#ifdef SMARTHUB_ENABLE_LVGL
    std::unique_ptr<UIManager> m_uiManager;
#endif
};

} // namespace smarthub
