/**
 * SmartHub Application Implementation
 */

#include <smarthub/core/Application.hpp>
#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/core/Config.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/web/WebServer.hpp>
#include <smarthub/protocols/mqtt/MqttClient.hpp>
#include <smarthub/rpmsg/RpmsgClient.hpp>
#include <smarthub/security/ApiTokenManager.hpp>
#include <smarthub/security/CertManager.hpp>
#include <smarthub/security/SessionManager.hpp>
#include <smarthub/security/UserManager.hpp>
#include <nlohmann/json.hpp>

#ifdef SMARTHUB_ENABLE_LVGL
#include <smarthub/ui/UIManager.hpp>
#endif

#include <chrono>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace smarthub {

Application::Application(const std::string& configPath)
    : m_configPath(configPath)
{
}

Application::~Application() {
    // This can run while an exception unwinds, when a second one would end the program
    try {
        shutdown();
    } catch (const std::exception& e) {
        LOG_ERROR("Error during shutdown: %s", e.what());
    }
}

bool Application::initialize() {
    LOG_INFO("SmartHub v%s initializing...", version());
    m_shutDown = false;

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

    // Initialize Zigbee if enabled
    LOG_INFO("Zigbee config: enabled=%d port=%s baud=%d",
             m_config->zigbeeEnabled(),
             m_config->zigbeePort().c_str(),
             m_config->zigbeeBaudRate());

    if (m_config->zigbeeEnabled()) {
        nlohmann::json zigbeeConfig;
        zigbeeConfig["port"] = m_config->zigbeePort();
        zigbeeConfig["baudRate"] = m_config->zigbeeBaudRate();

        LOG_INFO("Loading Zigbee protocol handler...");
        if (!m_deviceManager->loadProtocol("zigbee", zigbeeConfig)) {
            LOG_WARN("Zigbee initialization failed, check coordinator connection");
        } else {
            LOG_INFO("Zigbee protocol loaded successfully");
        }
    } else {
        LOG_INFO("Zigbee disabled in config");
    }

    if (!initializeMqtt()) {
        // MQTT failure is not fatal - will retry
        LOG_WARN("MQTT initialization failed, will retry later");
    }

    if (!initializeRpmsg()) {
        // RPMsg failure is not fatal - M4 might not be running
        LOG_WARN("RPMsg initialization failed, M4 may not be running");
    }

    if (!initializeSecurity()) {
        LOG_ERROR("Failed to initialize security");
        return false;
    }

    if (!initializeAutomation()) {
        LOG_ERROR("Failed to initialize automations");
        return false;
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

void Application::run(const std::atomic<bool>& stopRequested) {
    if (!m_initialized) {
        LOG_ERROR("Cannot run: application not initialized");
        return;
    }

    LOG_INFO("SmartHub running");

    // Publish system ready event
    SystemEvent readyEvent;
    readyEvent.status = SystemEvent::Status::Ready;
    readyEvent.message = "SmartHub ready";
    m_eventBus->publish(readyEvent);

    mainLoop(stopRequested);

    // Publish shutdown event
    SystemEvent shutdownEvent;
    shutdownEvent.status = SystemEvent::Status::ShuttingDown;
    shutdownEvent.message = "SmartHub shutting down";
    m_eventBus->publish(shutdownEvent);

    shutdown();
}

void Application::shutdown() {
    if (m_shutDown) {
        return;
    }
    m_shutDown = true;

    LOG_INFO("Shutting down SmartHub...");
    shutdownSubsystems();
    LOG_INFO("SmartHub shutdown complete");
}

void Application::mainLoop(const std::atomic<bool>& stopRequested) {
    using namespace std::chrono;

    const auto targetFrameTime = milliseconds(5); // ~200 Hz for LVGL
    auto lastAutomationPoll = steady_clock::now();

    while (!stopRequested) {
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

        // Run time-based automations. They work in minutes, so once a second is plenty.
        if (m_automationManager && frameStart - lastAutomationPoll >= seconds(1)) {
            lastAutomationPoll = frameStart;
            m_automationManager->poll(static_cast<uint64_t>(
                duration_cast<milliseconds>(frameStart.time_since_epoch()).count()));
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

bool Application::initializeAutomation() {
    m_automationManager = std::make_unique<AutomationManager>(
        *m_eventBus, *m_database, *m_deviceManager);

    if (!m_automationManager->initialize()) {
        return false;
    }

    LOG_INFO("Automation manager initialized");
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

bool Application::initializeSecurity() {
    m_userManager = std::make_unique<security::UserManager>(*m_database);
    if (!m_userManager->initialize()) {
        return false;
    }

    m_apiTokenManager = std::make_unique<security::ApiTokenManager>(*m_database);
    if (!m_apiTokenManager->initialize()) {
        return false;
    }

    int sessionTimeout = m_config->sessionTimeoutMinutes();
    if (sessionTimeout <= 0) {
        // Sessions would expire as they were created, locking everyone out
        LOG_WARN("Invalid session timeout %d, using 60 minutes", sessionTimeout);
        sessionTimeout = 60;
    }
    m_sessionManager = std::make_unique<security::SessionManager>(sessionTimeout);

    if (!m_userManager->hasAnyUser()) {
        LOG_WARN("No accounts exist yet: open the web dashboard to create the administrator");
    }
    return true;
}

bool Application::configureTls() {
    const std::string certPath = m_config->tlsCertPath();
    const std::string keyPath = m_config->tlsKeyPath();

    // Empty paths are how a config turns TLS off
    if (certPath.empty() && keyPath.empty()) {
        return true;
    }
    if (certPath.empty() || keyPath.empty()) {
        LOG_ERROR("TLS needs both a certificate and a key path");
        return false;
    }

    if (certPath == keyPath) {
        LOG_ERROR("TLS needs separate certificate and key files, but both are %s", certPath.c_str());
        return false;
    }

    // First boot, or one that lost power between writing the key and the certificate. A key on
    // its own is no use to anyone. A certificate that exists is left alone, even if it doesn't
    // work: it could be someone's real one, and regenerating would hide what's wrong with it.
    std::error_code error;  // Not being able to check counts as not there, and creating it fails
    if (!fs::exists(certPath, error)) {
        if (fs::exists(keyPath, error)) {
            LOG_WARN("Found TLS key %s with no certificate, replacing it", keyPath.c_str());
        }
        LOG_INFO("Creating a self-signed TLS certificate at %s", certPath.c_str());
        if (!security::CertManager::createSelfSigned(certPath, keyPath, "smarthub.local")) {
            LOG_ERROR("Failed to create a TLS certificate at %s", certPath.c_str());
            return false;
        }
    }

    if (!security::CertManager::isUsablePair(certPath, keyPath)) {
        LOG_ERROR("TLS certificate %s and key %s can't be used", certPath.c_str(), keyPath.c_str());
        return false;
    }

    m_webServer->setTlsCert(certPath, keyPath);
    m_webServer->setHttpRedirect(true, m_config->webHttpPort());  // Redirect HTTP to HTTPS
    return true;
}

bool Application::initializeWebServer() {
    m_webServer = std::make_unique<WebServer>(
        *m_eventBus,
        *m_deviceManager,
        m_config->webPort(),
        m_config->webRoot()
    );

    if (!configureTls()) {
        // No web server rather than an insecure one. Everything else, including automations and
        // the touchscreen, doesn't depend on a certificate, so it keeps running.
        LOG_ERROR("Web server not started, because TLS can't be used. "
                  "Fix the certificate, or set both paths to empty to run without TLS.");
        m_webServer.reset();
        return true;
    }

    m_webServer->setSecurityManagers(m_sessionManager.get(), m_apiTokenManager.get(),
                                     m_userManager.get());
    m_webServer->setAutomationManager(m_automationManager.get());
    m_webServer->setRateLimit(m_config->rateLimitPerMinute());
    int loginWindow = m_config->loginFailureWindowSeconds();
    if (loginWindow <= 0) {
        // No window would mean the failure count never builds up
        LOG_WARN("Invalid login failure window %d, using 300 seconds", loginWindow);
        loginWindow = 300;
    }
    m_webServer->setLoginThrottle(m_config->loginMaxFailures(), loginWindow);
    m_webServer->setAllowedHosts(m_config->allowedHosts());

    if (!m_webServer->start()) {
        return false;
    }

    LOG_INFO("Web server started on port %d", m_config->webPort());
    return true;
}

bool Application::initializeUI() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_uiManager = std::make_unique<UIManager>(*m_eventBus, *m_deviceManager, *m_database,
                                              m_automationManager.get());

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
    // First stop automations, so a run that's waiting in a delay can't hold up the threads
    // being stopped below, or use anything after it's gone.
    if (m_automationManager) {
        m_automationManager->shutdown();
    }

    if (m_webServer) {
        m_webServer->stop();
        m_webServer.reset();
    }

    m_apiTokenManager.reset();
    m_sessionManager.reset();
    m_userManager.reset();

    if (m_rpmsgClient) {
        m_rpmsgClient->shutdown();
        m_rpmsgClient.reset();
    }

    if (m_mqttClient) {
        m_mqttClient->disconnect();
        m_mqttClient.reset();
    }

    // This joins the protocol threads, which call into automations and the UI's screens
    if (m_deviceManager) {
        m_deviceManager->shutdown();
    }

#ifdef SMARTHUB_ENABLE_LVGL
    if (m_uiManager) {
        m_uiManager->shutdown();
        m_uiManager.reset();
    }
#endif

    m_automationManager.reset();
    m_deviceManager.reset();

    if (m_database) {
        m_database->close();
        m_database.reset();
    }

    m_eventBus.reset();
    m_config.reset();
    m_initialized = false;
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
