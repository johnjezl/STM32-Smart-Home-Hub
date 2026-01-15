/**
 * SmartHub Application Implementation
 */

#include <smarthub/core/Application.hpp>
#include <smarthub/core/Config.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/web/WebServer.hpp>
#include <smarthub/protocols/mqtt/MqttClient.hpp>
#include <smarthub/rpmsg/RpmsgClient.hpp>

#ifdef SMARTHUB_ENABLE_LVGL
#include <smarthub/ui/UIManager.hpp>
#endif

#include <thread>
#include <chrono>

namespace smarthub {

Application::Application(const std::string& configPath)
    : m_configPath(configPath)
{
}

Application::~Application() {
    if (m_running) {
        shutdown();
    }
}

bool Application::initialize() {
    LOG_INFO("SmartHub v%s initializing...", version());

    // Initialize in dependency order
    if (!initializeConfig()) {
        LOG_ERROR("Failed to initialize configuration");
        return false;
    }

    // Create event bus
    m_eventBus = std::make_unique<EventBus>();
    LOG_INFO("Event bus initialized");

    if (!initializeDatabase()) {
        LOG_ERROR("Failed to initialize database");
        return false;
    }

    if (!initializeDeviceManager()) {
        LOG_ERROR("Failed to initialize device manager");
        return false;
    }

    if (!initializeMqtt()) {
        // MQTT failure is not fatal - will retry
        LOG_WARN("MQTT initialization failed, will retry later");
    }

    if (!initializeRpmsg()) {
        // RPMsg failure is not fatal - M4 might not be running
        LOG_WARN("RPMsg initialization failed, M4 may not be running");
    }

    if (!initializeWebServer()) {
        LOG_ERROR("Failed to initialize web server");
        return false;
    }

    if (!initializeUI()) {
        // UI failure is not fatal on headless builds
        LOG_WARN("UI initialization failed or disabled");
    }

    m_initialized = true;
    LOG_INFO("SmartHub initialization complete");
    return true;
}

void Application::run() {
    if (!m_initialized) {
        LOG_ERROR("Cannot run: application not initialized");
        return;
    }

    m_running = true;
    LOG_INFO("SmartHub running");

    // Publish system ready event
    SystemEvent readyEvent;
    readyEvent.status = SystemEvent::Status::Ready;
    readyEvent.message = "SmartHub ready";
    m_eventBus->publish(readyEvent);

    mainLoop();

    // Publish shutdown event
    SystemEvent shutdownEvent;
    shutdownEvent.status = SystemEvent::Status::ShuttingDown;
    shutdownEvent.message = "SmartHub shutting down";
    m_eventBus->publish(shutdownEvent);
}

void Application::shutdown() {
    if (!m_running.exchange(false)) {
        return; // Already shut down
    }

    LOG_INFO("Shutting down SmartHub...");
    shutdownSubsystems();
    LOG_INFO("SmartHub shutdown complete");
}

void Application::mainLoop() {
    using namespace std::chrono;

    const auto targetFrameTime = milliseconds(5); // ~200 Hz for LVGL

    while (m_running) {
        auto frameStart = steady_clock::now();

        // Process async events
        m_eventBus->processQueue();

        // Update UI
#ifdef SMARTHUB_ENABLE_LVGL
        if (m_uiManager) {
            m_uiManager->update();
        }
#endif

        // Poll MQTT
        if (m_mqttClient && m_mqttClient->isConnected()) {
            m_mqttClient->poll();
        }

        // Poll RPMsg
        if (m_rpmsgClient && m_rpmsgClient->isConnected()) {
            m_rpmsgClient->poll();
        }

        // Sleep to maintain frame rate
        auto frameEnd = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(frameEnd - frameStart);
        if (elapsed < targetFrameTime) {
            std::this_thread::sleep_for(targetFrameTime - elapsed);
        }
    }
}

bool Application::initializeConfig() {
    m_config = std::make_unique<Config>();

    if (!m_config->load(m_configPath)) {
        LOG_WARN("Could not load config from %s, using defaults", m_configPath.c_str());
    }

    // Re-initialize logger with config settings
    Logger::Level logLevel = Logger::Level::Info;
    std::string levelStr = m_config->logLevel();
    if (levelStr == "debug") logLevel = Logger::Level::Debug;
    else if (levelStr == "warning" || levelStr == "warn") logLevel = Logger::Level::Warning;
    else if (levelStr == "error") logLevel = Logger::Level::Error;

    Logger::init(logLevel, m_config->logFile());
    LOG_INFO("Configuration loaded from %s", m_configPath.c_str());

    return true;
}

bool Application::initializeDatabase() {
    m_database = std::make_unique<Database>(m_config->databasePath());

    if (!m_database->initialize()) {
        return false;
    }

    LOG_INFO("Database initialized at %s", m_config->databasePath().c_str());
    return true;
}

bool Application::initializeDeviceManager() {
    m_deviceManager = std::make_unique<DeviceManager>(*m_eventBus, *m_database);

    if (!m_deviceManager->initialize()) {
        return false;
    }

    LOG_INFO("Device manager initialized");
    return true;
}

bool Application::initializeMqtt() {
    m_mqttClient = std::make_unique<MqttClient>(
        *m_eventBus,
        m_config->mqttBroker(),
        m_config->mqttPort()
    );

    if (!m_mqttClient->connect()) {
        return false;
    }

    LOG_INFO("MQTT client connected to %s:%d",
             m_config->mqttBroker().c_str(), m_config->mqttPort());
    return true;
}

bool Application::initializeRpmsg() {
    m_rpmsgClient = std::make_unique<RpmsgClient>(*m_eventBus, m_config->rpmsgDevice());

    if (!m_rpmsgClient->initialize()) {
        return false;
    }

    LOG_INFO("RPMsg client initialized on %s", m_config->rpmsgDevice().c_str());
    return true;
}

bool Application::initializeWebServer() {
    m_webServer = std::make_unique<WebServer>(
        *m_eventBus,
        *m_deviceManager,
        m_config->webPort(),
        m_config->webRoot()
    );

    // Configure TLS if cert and key paths are set
    std::string certPath = m_config->tlsCertPath();
    std::string keyPath = m_config->tlsKeyPath();
    if (!certPath.empty() && !keyPath.empty()) {
        m_webServer->setTlsCert(certPath, keyPath);
        m_webServer->setHttpRedirect(true, 80);  // Redirect HTTP to HTTPS
    }

    if (!m_webServer->start()) {
        return false;
    }

    LOG_INFO("Web server started on port %d", m_config->webPort());
    return true;
}

bool Application::initializeUI() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_uiManager = std::make_unique<UIManager>(*m_eventBus, *m_deviceManager, *m_database);

    if (!m_uiManager->initialize()) {
        return false;
    }

    LOG_INFO("UI manager initialized");
    return true;
#else
    LOG_INFO("UI disabled (LVGL not available)");
    return true;
#endif
}

void Application::shutdownSubsystems() {
    // Shutdown in reverse order of initialization

#ifdef SMARTHUB_ENABLE_LVGL
    if (m_uiManager) {
        m_uiManager->shutdown();
        m_uiManager.reset();
    }
#endif

    if (m_webServer) {
        m_webServer->stop();
        m_webServer.reset();
    }

    if (m_rpmsgClient) {
        m_rpmsgClient->shutdown();
        m_rpmsgClient.reset();
    }

    if (m_mqttClient) {
        m_mqttClient->disconnect();
        m_mqttClient.reset();
    }

    if (m_deviceManager) {
        m_deviceManager->shutdown();
        m_deviceManager.reset();
    }

    if (m_database) {
        m_database->close();
        m_database.reset();
    }

    m_eventBus.reset();
    m_config.reset();
}

Config& Application::config() {
    return *m_config;
}

EventBus& Application::eventBus() {
    return *m_eventBus;
}

Database& Application::database() {
    return *m_database;
}

DeviceManager& Application::deviceManager() {
    return *m_deviceManager;
}

} // namespace smarthub
