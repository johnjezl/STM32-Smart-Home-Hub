# Phase 2: Core Application Framework - Detailed TODO

## Overview
**Duration**: 2-3 weeks  
**Objective**: Implement the foundational C++ application architecture that all features build upon.  
**Prerequisites**: Phase 1 complete (bootable Buildroot system)

---

## 2.1 Project Structure Setup

### 2.1.1 Create Application Directory Structure
```bash
cd ~/projects/smarthub
mkdir -p app/{src,include,assets,tests}
mkdir -p app/src/{core,database,devices,protocols,ui,web,rpmsg,utils}
mkdir -p app/include/smarthub/{core,database,devices,protocols,ui,web,rpmsg,utils}
mkdir -p app/assets/{fonts,images,icons}
```

### 2.1.2 Root CMakeLists.txt
```bash
cat > app/CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.16)
project(SmartHub VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(SMARTHUB_BUILD_TESTS "Build unit tests" ON)
option(SMARTHUB_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2)
endif()

if(SMARTHUB_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

# LVGL
pkg_check_modules(LVGL REQUIRED lvgl)
pkg_check_modules(LV_DRIVERS REQUIRED lv_drivers)

# Mosquitto
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LVGL_INCLUDE_DIRS}
    ${LV_DRIVERS_INCLUDE_DIRS}
    ${MOSQUITTO_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Main executable
add_executable(smarthub ${SOURCES})

target_link_libraries(smarthub
    PRIVATE
        Threads::Threads
        SQLite::SQLite3
        OpenSSL::SSL
        OpenSSL::Crypto
        ${LVGL_LIBRARIES}
        ${LV_DRIVERS_LIBRARIES}
        ${MOSQUITTO_LIBRARIES}
        nlohmann_json::nlohmann_json
        yaml-cpp
        dl  # For plugin loading
)

# Install
install(TARGETS smarthub DESTINATION /opt/smarthub)
install(DIRECTORY assets/ DESTINATION /opt/smarthub/assets)

# Tests
if(SMARTHUB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
EOF
```

### 2.1.3 Cross-Compilation Toolchain File
```bash
cat > app/cmake/arm-linux-gnueabihf.cmake << 'EOF'
# Cross-compilation toolchain for STM32MP1

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Where to look for the target environment
set(CMAKE_FIND_ROOT_PATH ${BUILDROOT_SYSROOT})

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
```

---

## 2.2 Core Infrastructure

### 2.2.1 Main Entry Point
```cpp
// app/src/main.cpp
#include <smarthub/core/Application.hpp>
#include <smarthub/core/Logger.hpp>
#include <csignal>
#include <cstdlib>

using namespace smarthub;

static Application* g_app = nullptr;

void signalHandler(int signal) {
    LOG_INFO("Received signal {}, shutting down...", signal);
    if (g_app) {
        g_app->shutdown();
    }
}

int main(int argc, char* argv[]) {
    // Initialize logging
    Logger::init(Logger::Level::Info, "/var/log/smarthub.log");
    LOG_INFO("SmartHub starting...");
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    std::string configPath = "/etc/smarthub/config.yaml";
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-c" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }
    
    // Create and run application
    try {
        Application app(configPath);
        g_app = &app;
        
        if (!app.initialize()) {
            LOG_ERROR("Failed to initialize application");
            return EXIT_FAILURE;
        }
        
        app.run();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }
    
    LOG_INFO("SmartHub shutdown complete");
    return EXIT_SUCCESS;
}
```

### 2.2.2 Application Class Header
```cpp
// app/include/smarthub/core/Application.hpp
#pragma once

#include <string>
#include <memory>
#include <atomic>

namespace smarthub {

class Config;
class EventBus;
class Database;
class DeviceManager;
class WebServer;
class MqttClient;
class UIManager;
class RpmsgClient;

class Application {
public:
    explicit Application(const std::string& configPath);
    ~Application();
    
    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    bool initialize();
    void run();
    void shutdown();
    
    // Component accessors
    Config& config();
    EventBus& eventBus();
    Database& database();
    DeviceManager& deviceManager();
    
private:
    std::string m_configPath;
    std::atomic<bool> m_running{false};
    
    std::unique_ptr<Config> m_config;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<DeviceManager> m_deviceManager;
    std::unique_ptr<WebServer> m_webServer;
    std::unique_ptr<MqttClient> m_mqttClient;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<RpmsgClient> m_rpmsgClient;
};

} // namespace smarthub
```

### 2.2.3 Application Class Implementation
```cpp
// app/src/core/Application.cpp
#include <smarthub/core/Application.hpp>
#include <smarthub/core/Config.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/web/WebServer.hpp>
#include <smarthub/protocols/mqtt/MqttClient.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/rpmsg/RpmsgClient.hpp>
#include <thread>
#include <chrono>

namespace smarthub {

Application::Application(const std::string& configPath)
    : m_configPath(configPath)
{
}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    LOG_INFO("Initializing SmartHub...");
    
    // Load configuration
    m_config = std::make_unique<Config>();
    if (!m_config->load(m_configPath)) {
        LOG_ERROR("Failed to load configuration from {}", m_configPath);
        return false;
    }
    LOG_INFO("Configuration loaded");
    
    // Initialize event bus
    m_eventBus = std::make_unique<EventBus>();
    LOG_INFO("Event bus initialized");
    
    // Initialize database
    m_database = std::make_unique<Database>(m_config->databasePath());
    if (!m_database->initialize()) {
        LOG_ERROR("Failed to initialize database");
        return false;
    }
    LOG_INFO("Database initialized");
    
    // Initialize device manager
    m_deviceManager = std::make_unique<DeviceManager>(*m_eventBus, *m_database);
    if (!m_deviceManager->initialize()) {
        LOG_ERROR("Failed to initialize device manager");
        return false;
    }
    LOG_INFO("Device manager initialized");
    
    // Initialize MQTT client
    m_mqttClient = std::make_unique<MqttClient>(
        *m_eventBus,
        m_config->mqttBroker(),
        m_config->mqttPort()
    );
    if (!m_mqttClient->connect()) {
        LOG_WARN("Failed to connect to MQTT broker - will retry");
    }
    LOG_INFO("MQTT client initialized");
    
    // Initialize RPMsg client for M4 communication
    m_rpmsgClient = std::make_unique<RpmsgClient>(*m_eventBus);
    if (!m_rpmsgClient->initialize()) {
        LOG_WARN("Failed to initialize RPMsg - M4 may not be running");
    }
    LOG_INFO("RPMsg client initialized");
    
    // Initialize web server
    m_webServer = std::make_unique<WebServer>(
        *m_eventBus,
        *m_deviceManager,
        m_config->webPort(),
        m_config->webRoot()
    );
    if (!m_webServer->start()) {
        LOG_ERROR("Failed to start web server");
        return false;
    }
    LOG_INFO("Web server started on port {}", m_config->webPort());
    
    // Initialize UI manager (LVGL)
    m_uiManager = std::make_unique<UIManager>(*m_eventBus, *m_deviceManager);
    if (!m_uiManager->initialize()) {
        LOG_ERROR("Failed to initialize UI");
        return false;
    }
    LOG_INFO("UI initialized");
    
    LOG_INFO("SmartHub initialization complete");
    return true;
}

void Application::run() {
    m_running = true;
    LOG_INFO("SmartHub running");
    
    while (m_running) {
        // Process LVGL tasks
        m_uiManager->update();
        
        // Process MQTT messages
        m_mqttClient->poll();
        
        // Process RPMsg messages
        m_rpmsgClient->poll();
        
        // Small delay to prevent busy-spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void Application::shutdown() {
    if (!m_running.exchange(false)) {
        return; // Already shut down
    }
    
    LOG_INFO("Shutting down SmartHub...");
    
    // Shutdown in reverse order of initialization
    if (m_uiManager) m_uiManager->shutdown();
    if (m_webServer) m_webServer->stop();
    if (m_rpmsgClient) m_rpmsgClient->shutdown();
    if (m_mqttClient) m_mqttClient->disconnect();
    if (m_deviceManager) m_deviceManager->shutdown();
    if (m_database) m_database->close();
}

Config& Application::config() { return *m_config; }
EventBus& Application::eventBus() { return *m_eventBus; }
Database& Application::database() { return *m_database; }
DeviceManager& Application::deviceManager() { return *m_deviceManager; }

} // namespace smarthub
```

### 2.2.4 Logger Implementation
```cpp
// app/include/smarthub/core/Logger.hpp
#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <cstdio>
#include <ctime>

namespace smarthub {

class Logger {
public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error
    };
    
    static void init(Level minLevel, const std::string& logFile = "");
    static Logger& instance();
    
    template<typename... Args>
    void log(Level level, const char* file, int line, const char* fmt, Args&&... args) {
        if (level < m_minLevel) return;
        
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
        
        writeLog(level, file, line, buffer);
    }
    
private:
    Logger() = default;
    void writeLog(Level level, const char* file, int line, const char* message);
    
    Level m_minLevel = Level::Info;
    std::ofstream m_logFile;
    std::mutex m_mutex;
    static std::unique_ptr<Logger> s_instance;
};

// Helper macros
#define LOG_DEBUG(...) smarthub::Logger::instance().log(smarthub::Logger::Level::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) smarthub::Logger::instance().log(smarthub::Logger::Level::Info, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) smarthub::Logger::instance().log(smarthub::Logger::Level::Warning, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) smarthub::Logger::instance().log(smarthub::Logger::Level::Error, __FILE__, __LINE__, __VA_ARGS__)

} // namespace smarthub
```

### 2.2.5 Event Bus Implementation
```cpp
// app/include/smarthub/core/EventBus.hpp
#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <any>
#include <typeindex>

namespace smarthub {

class Event {
public:
    virtual ~Event() = default;
    virtual std::string type() const = 0;
};

// Device state changed event
class DeviceStateEvent : public Event {
public:
    std::string deviceId;
    std::string property;
    std::any value;
    
    std::string type() const override { return "device.state"; }
};

// Sensor data event (from M4)
class SensorDataEvent : public Event {
public:
    std::string sensorId;
    std::string sensorType;
    double value;
    uint64_t timestamp;
    
    std::string type() const override { return "sensor.data"; }
};

// MQTT message event
class MqttMessageEvent : public Event {
public:
    std::string topic;
    std::string payload;
    
    std::string type() const override { return "mqtt.message"; }
};

using EventHandler = std::function<void(const Event&)>;
using SubscriptionId = uint64_t;

class EventBus {
public:
    EventBus() = default;
    
    // Subscribe to event type
    SubscriptionId subscribe(const std::string& eventType, EventHandler handler);
    
    // Unsubscribe
    void unsubscribe(SubscriptionId id);
    
    // Publish event
    void publish(const Event& event);
    
    // Publish event async (queued)
    void publishAsync(std::unique_ptr<Event> event);
    
    // Process queued events (call from main loop)
    void processQueue();
    
private:
    struct Subscription {
        SubscriptionId id;
        EventHandler handler;
    };
    
    std::unordered_map<std::string, std::vector<Subscription>> m_subscribers;
    std::vector<std::unique_ptr<Event>> m_eventQueue;
    std::mutex m_mutex;
    std::mutex m_queueMutex;
    SubscriptionId m_nextId = 1;
};

} // namespace smarthub
```

### 2.2.6 Configuration System
```cpp
// app/include/smarthub/core/Config.hpp
#pragma once

#include <string>
#include <yaml-cpp/yaml.h>

namespace smarthub {

class Config {
public:
    bool load(const std::string& path);
    bool save(const std::string& path);
    
    // Database
    std::string databasePath() const { return m_databasePath; }
    
    // MQTT
    std::string mqttBroker() const { return m_mqttBroker; }
    int mqttPort() const { return m_mqttPort; }
    
    // Web server
    int webPort() const { return m_webPort; }
    std::string webRoot() const { return m_webRoot; }
    std::string tlsCertPath() const { return m_tlsCertPath; }
    std::string tlsKeyPath() const { return m_tlsKeyPath; }
    
    // Display
    std::string displayDevice() const { return m_displayDevice; }
    std::string touchDevice() const { return m_touchDevice; }
    
    // RPMsg
    std::string rpmsgDevice() const { return m_rpmsgDevice; }
    
private:
    void setDefaults();
    
    std::string m_databasePath = "/var/lib/smarthub/smarthub.db";
    std::string m_mqttBroker = "127.0.0.1";
    int m_mqttPort = 1883;
    int m_webPort = 443;
    std::string m_webRoot = "/opt/smarthub/www";
    std::string m_tlsCertPath = "/etc/smarthub/cert.pem";
    std::string m_tlsKeyPath = "/etc/smarthub/key.pem";
    std::string m_displayDevice = "/dev/dri/card0";
    std::string m_touchDevice = "/dev/input/event0";
    std::string m_rpmsgDevice = "/dev/ttyRPMSG0";
};

} // namespace smarthub
```

### 2.2.7 Default Configuration File
```yaml
# app/assets/config.yaml
# SmartHub Configuration

# Database settings
database:
  path: /var/lib/smarthub/smarthub.db

# MQTT Broker connection
mqtt:
  broker: 127.0.0.1
  port: 1883
  client_id: smarthub
  username: ""
  password: ""

# Web server settings
web:
  port: 443
  http_port: 80  # Redirect to HTTPS
  root: /opt/smarthub/www
  tls:
    cert: /etc/smarthub/cert.pem
    key: /etc/smarthub/key.pem

# Display settings
display:
  device: /dev/dri/card0
  touch_device: /dev/input/event0
  brightness: 100
  screen_timeout: 60  # seconds, 0 to disable

# M4 communication
rpmsg:
  device: /dev/ttyRPMSG0
  baud: 115200

# Zigbee settings (Phase 4)
zigbee:
  enabled: false
  port: /dev/ttyUSB0
  baud: 115200

# Logging
logging:
  level: info  # debug, info, warning, error
  file: /var/log/smarthub.log
  max_size: 10485760  # 10 MB
  max_files: 3
```

---

## 2.3 Database Layer

### 2.3.1 Database Wrapper
```cpp
// app/include/smarthub/database/Database.hpp
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <sqlite3.h>

namespace smarthub {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    
    bool initialize();
    void close();
    
    // Execute SQL without results
    bool execute(const std::string& sql);
    
    // Query with callback
    template<typename Callback>
    bool query(const std::string& sql, Callback callback);
    
    // Prepared statements
    class Statement {
    public:
        Statement(sqlite3* db, const std::string& sql);
        ~Statement();
        
        Statement& bind(int index, int value);
        Statement& bind(int index, double value);
        Statement& bind(int index, const std::string& value);
        Statement& bindNull(int index);
        
        bool execute();
        bool step();
        void reset();
        
        int getInt(int column);
        double getDouble(int column);
        std::string getString(int column);
        bool isNull(int column);
        
    private:
        sqlite3_stmt* m_stmt = nullptr;
    };
    
    std::unique_ptr<Statement> prepare(const std::string& sql);
    
    // Transaction support
    bool beginTransaction();
    bool commit();
    bool rollback();
    
    // Get last error
    std::string lastError() const;
    
private:
    bool createSchema();
    
    std::string m_path;
    sqlite3* m_db = nullptr;
};

} // namespace smarthub
```

### 2.3.2 Database Schema
```cpp
// app/src/database/Database.cpp (partial - schema creation)

bool Database::createSchema() {
    const char* schema = R"SQL(
        -- Devices table
        CREATE TABLE IF NOT EXISTS devices (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            type TEXT NOT NULL,
            protocol TEXT NOT NULL,
            protocol_address TEXT,
            room TEXT,
            config TEXT,  -- JSON
            created_at INTEGER DEFAULT (strftime('%s', 'now')),
            updated_at INTEGER DEFAULT (strftime('%s', 'now'))
        );
        
        -- Device state (current values)
        CREATE TABLE IF NOT EXISTS device_state (
            device_id TEXT NOT NULL,
            property TEXT NOT NULL,
            value TEXT,
            updated_at INTEGER DEFAULT (strftime('%s', 'now')),
            PRIMARY KEY (device_id, property),
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        );
        
        -- Sensor history (time series)
        CREATE TABLE IF NOT EXISTS sensor_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            property TEXT NOT NULL,
            value REAL NOT NULL,
            timestamp INTEGER NOT NULL,
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_sensor_history_device_time 
            ON sensor_history(device_id, timestamp);
        
        -- Rooms
        CREATE TABLE IF NOT EXISTS rooms (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            icon TEXT,
            sort_order INTEGER DEFAULT 0
        );
        
        -- Automations (future)
        CREATE TABLE IF NOT EXISTS automations (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            trigger_config TEXT NOT NULL,  -- JSON
            action_config TEXT NOT NULL,   -- JSON
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        );
        
        -- User accounts (for web UI)
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT DEFAULT 'user',
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        );
        
        -- API tokens
        CREATE TABLE IF NOT EXISTS api_tokens (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL,
            name TEXT,
            expires_at INTEGER,
            created_at INTEGER DEFAULT (strftime('%s', 'now')),
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );
        
        -- Settings (key-value store)
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT
        );
        
        -- Insert default settings
        INSERT OR IGNORE INTO settings (key, value) VALUES 
            ('system.name', 'SmartHub'),
            ('system.timezone', 'UTC'),
            ('display.theme', 'dark'),
            ('display.brightness', '100');
    )SQL";
    
    return execute(schema);
}
```

---

## 2.4 LVGL Integration

### 2.4.1 UI Manager
```cpp
// app/include/smarthub/ui/UIManager.hpp
#pragma once

#include <memory>
#include <string>
#include <lvgl.h>

namespace smarthub {

class EventBus;
class DeviceManager;
class Screen;

class UIManager {
public:
    UIManager(EventBus& eventBus, DeviceManager& deviceManager);
    ~UIManager();
    
    bool initialize();
    void shutdown();
    void update();  // Call from main loop (handles lv_timer_handler)
    
    // Screen navigation
    void showScreen(const std::string& screenName);
    void goBack();
    
    // Display control
    void setBrightness(int percent);
    void setScreenTimeout(int seconds);
    void wakeScreen();
    
private:
    bool initDisplay();
    bool initInput();
    void createScreens();
    void setupTheme();
    
    EventBus& m_eventBus;
    DeviceManager& m_deviceManager;
    
    lv_display_t* m_display = nullptr;
    lv_indev_t* m_touchpad = nullptr;
    
    std::unique_ptr<Screen> m_currentScreen;
    std::vector<std::unique_ptr<Screen>> m_screens;
    
    uint32_t m_lastActivityTime = 0;
    int m_screenTimeout = 60;
    bool m_screenOn = true;
};

} // namespace smarthub
```

### 2.4.2 LVGL Display Driver Setup
```cpp
// app/src/ui/UIManager.cpp (partial - display init)

#include <smarthub/ui/UIManager.hpp>
#include <smarthub/core/Logger.hpp>
#include <lvgl.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <unistd.h>

namespace smarthub {

// Frame buffer info
static int fb_fd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static char* fb_mem = nullptr;
static uint32_t fb_size = 0;

// Display flush callback
static void fbdev_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    
    uint32_t* fb_ptr = (uint32_t*)fb_mem;
    uint32_t* src = (uint32_t*)px_map;
    
    for (int32_t y = 0; y < h; y++) {
        uint32_t* dst = fb_ptr + (area->y1 + y) * vinfo.xres + area->x1;
        memcpy(dst, src, w * sizeof(uint32_t));
        src += w;
    }
    
    lv_display_flush_ready(disp);
}

bool UIManager::initDisplay() {
    // Open framebuffer
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        LOG_ERROR("Failed to open framebuffer");
        return false;
    }
    
    // Get variable screen info
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        LOG_ERROR("Failed to get framebuffer info");
        close(fb_fd);
        return false;
    }
    
    // Get fixed screen info
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        LOG_ERROR("Failed to get framebuffer fixed info");
        close(fb_fd);
        return false;
    }
    
    LOG_INFO("Display: {}x{}, {} bpp", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    
    // Map framebuffer to memory
    fb_size = vinfo.yres_virtual * finfo.line_length;
    fb_mem = (char*)mmap(nullptr, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        LOG_ERROR("Failed to mmap framebuffer");
        close(fb_fd);
        return false;
    }
    
    // Initialize LVGL
    lv_init();
    
    // Create display
    m_display = lv_display_create(vinfo.xres, vinfo.yres);
    lv_display_set_flush_cb(m_display, fbdev_flush);
    
    // Allocate draw buffers
    static uint8_t buf1[480 * 100 * 4];  // Adjust size as needed
    static uint8_t buf2[480 * 100 * 4];
    lv_display_set_buffers(m_display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    return true;
}

} // namespace smarthub
```

### 2.4.3 Touch Input Driver
```cpp
// app/src/ui/UIManager.cpp (partial - touch input)

#include <linux/input.h>

static int touch_fd = -1;
static int touch_x = 0, touch_y = 0;
static bool touch_pressed = false;

static void touchpad_read(lv_indev_t* indev, lv_indev_data_t* data) {
    struct input_event ev;
    
    while (read(touch_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                touch_x = ev.value;
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                touch_y = ev.value;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            touch_pressed = (ev.value == 1);
        }
    }
    
    data->point.x = touch_x;
    data->point.y = touch_y;
    data->state = touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool UIManager::initInput() {
    // Open touch device
    touch_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (touch_fd < 0) {
        LOG_ERROR("Failed to open touch device");
        return false;
    }
    
    // Create input device
    m_touchpad = lv_indev_create();
    lv_indev_set_type(m_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(m_touchpad, touchpad_read);
    
    LOG_INFO("Touch input initialized");
    return true;
}
```

### 2.4.4 Virtual Keyboard
```cpp
// app/include/smarthub/ui/widgets/VirtualKeyboard.hpp
#pragma once

#include <lvgl.h>
#include <functional>
#include <string>

namespace smarthub {

class VirtualKeyboard {
public:
    using TextCallback = std::function<void(const std::string&)>;
    
    VirtualKeyboard(lv_obj_t* parent);
    ~VirtualKeyboard();
    
    void show(const std::string& title, const std::string& initialText = "");
    void hide();
    
    void setCallback(TextCallback callback) { m_callback = callback; }
    void setPasswordMode(bool enabled);
    
private:
    static void keyboardEventCb(lv_event_t* e);
    static void textareaEventCb(lv_event_t* e);
    
    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_textarea = nullptr;
    lv_obj_t* m_keyboard = nullptr;
    lv_obj_t* m_okButton = nullptr;
    lv_obj_t* m_cancelButton = nullptr;
    
    TextCallback m_callback;
};

} // namespace smarthub
```

---

## 2.5 Web Server (Mongoose)

### 2.5.1 Web Server Implementation
```cpp
// app/include/smarthub/web/WebServer.hpp
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mongoose.h>

namespace smarthub {

class EventBus;
class DeviceManager;

class WebServer {
public:
    WebServer(EventBus& eventBus, DeviceManager& deviceManager, 
              int port, const std::string& webRoot);
    ~WebServer();
    
    bool start();
    void stop();
    
private:
    static void eventHandler(struct mg_connection* c, int ev, void* ev_data);
    
    void handleHttpRequest(struct mg_connection* c, struct mg_http_message* hm);
    void handleWebSocket(struct mg_connection* c, struct mg_ws_message* wm);
    
    // API endpoints
    void apiGetDevices(struct mg_connection* c);
    void apiGetDevice(struct mg_connection* c, const std::string& id);
    void apiSetDeviceState(struct mg_connection* c, const std::string& id, 
                           struct mg_http_message* hm);
    void apiGetSensorHistory(struct mg_connection* c, const std::string& id);
    void apiGetSystemStatus(struct mg_connection* c);
    
    EventBus& m_eventBus;
    DeviceManager& m_deviceManager;
    int m_port;
    std::string m_webRoot;
    
    struct mg_mgr m_mgr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};

} // namespace smarthub
```

### 2.5.2 Web Server Implementation
```cpp
// app/src/web/WebServer.cpp
#include <smarthub/web/WebServer.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/core/Logger.hpp>
#include <nlohmann/json.hpp>

namespace smarthub {

using json = nlohmann::json;

// Static instance pointer for callback
static WebServer* s_instance = nullptr;

WebServer::WebServer(EventBus& eventBus, DeviceManager& deviceManager,
                     int port, const std::string& webRoot)
    : m_eventBus(eventBus)
    , m_deviceManager(deviceManager)
    , m_port(port)
    , m_webRoot(webRoot)
{
    s_instance = this;
}

WebServer::~WebServer() {
    stop();
    s_instance = nullptr;
}

bool WebServer::start() {
    mg_mgr_init(&m_mgr);
    
    char addr[32];
    snprintf(addr, sizeof(addr), "https://0.0.0.0:%d", m_port);
    
    struct mg_tls_opts tls_opts = {};
    tls_opts.cert = mg_str("/etc/smarthub/cert.pem");
    tls_opts.key = mg_str("/etc/smarthub/key.pem");
    
    struct mg_connection* c = mg_https_listen(&m_mgr, addr, eventHandler, &tls_opts);
    if (!c) {
        LOG_ERROR("Failed to start web server on {}", addr);
        return false;
    }
    
    m_running = true;
    m_thread = std::thread([this]() {
        while (m_running) {
            mg_mgr_poll(&m_mgr, 100);
        }
    });
    
    LOG_INFO("Web server listening on {}", addr);
    return true;
}

void WebServer::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    mg_mgr_free(&m_mgr);
}

void WebServer::eventHandler(struct mg_connection* c, int ev, void* ev_data) {
    if (!s_instance) return;
    
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        s_instance->handleHttpRequest(c, hm);
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
        s_instance->handleWebSocket(c, wm);
    }
}

void WebServer::handleHttpRequest(struct mg_connection* c, struct mg_http_message* hm) {
    std::string uri(hm->uri.buf, hm->uri.len);
    
    // API routes
    if (uri.starts_with("/api/")) {
        if (mg_match(hm->uri, mg_str("/api/devices"), nullptr)) {
            if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
                apiGetDevices(c);
            }
        } else if (mg_match(hm->uri, mg_str("/api/devices/*"), nullptr)) {
            std::string id = uri.substr(13);  // After "/api/devices/"
            if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
                apiGetDevice(c, id);
            } else if (mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                apiSetDeviceState(c, id, hm);
            }
        } else if (mg_match(hm->uri, mg_str("/api/system/status"), nullptr)) {
            apiGetSystemStatus(c);
        } else {
            mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                          "{\"error\":\"Not found\"}");
        }
    }
    // WebSocket upgrade
    else if (mg_match(hm->uri, mg_str("/ws"), nullptr)) {
        mg_ws_upgrade(c, hm, nullptr);
    }
    // Static files
    else {
        struct mg_http_serve_opts opts = {};
        opts.root_dir = m_webRoot.c_str();
        mg_http_serve_dir(c, hm, &opts);
    }
}

void WebServer::apiGetDevices(struct mg_connection* c) {
    json response = json::array();
    
    for (const auto& device : m_deviceManager.getAllDevices()) {
        response.push_back({
            {"id", device->id()},
            {"name", device->name()},
            {"type", device->type()},
            {"state", device->getState()}
        });
    }
    
    std::string body = response.dump();
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());
}

void WebServer::apiSetDeviceState(struct mg_connection* c, const std::string& id,
                                   struct mg_http_message* hm) {
    auto device = m_deviceManager.getDevice(id);
    if (!device) {
        mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                      "{\"error\":\"Device not found\"}");
        return;
    }
    
    std::string body(hm->body.buf, hm->body.len);
    try {
        json req = json::parse(body);
        
        for (auto& [key, value] : req.items()) {
            device->setState(key, value);
        }
        
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                      "{\"success\":true}");
    } catch (const std::exception& e) {
        mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                      "{\"error\":\"Invalid JSON\"}");
    }
}

} // namespace smarthub
```

---

## 2.6 MQTT Client

### 2.6.1 MQTT Client Implementation
```cpp
// app/include/smarthub/protocols/mqtt/MqttClient.hpp
#pragma once

#include <string>
#include <functional>
#include <mosquitto.h>

namespace smarthub {

class EventBus;

class MqttClient {
public:
    MqttClient(EventBus& eventBus, const std::string& broker, int port);
    ~MqttClient();
    
    bool connect();
    void disconnect();
    void poll();  // Call from main loop
    
    bool subscribe(const std::string& topic);
    bool unsubscribe(const std::string& topic);
    bool publish(const std::string& topic, const std::string& payload, bool retain = false);
    
    bool isConnected() const { return m_connected; }
    
private:
    static void onConnect(struct mosquitto* mosq, void* obj, int rc);
    static void onDisconnect(struct mosquitto* mosq, void* obj, int rc);
    static void onMessage(struct mosquitto* mosq, void* obj, 
                          const struct mosquitto_message* msg);
    
    EventBus& m_eventBus;
    std::string m_broker;
    int m_port;
    
    struct mosquitto* m_mosq = nullptr;
    bool m_connected = false;
};

} // namespace smarthub
```

---

## 2.7 Build and Test

### 2.7.1 Build Script
```bash
cat > ~/projects/smarthub/tools/build-app.sh << 'EOF'
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/app/build"

# Check for cross-compile toolchain
if [ -n "$BUILDROOT_SYSROOT" ]; then
    echo "Cross-compiling for ARM..."
    CMAKE_OPTS="-DCMAKE_TOOLCHAIN_FILE=$PROJECT_DIR/app/cmake/arm-linux-gnueabihf.cmake"
else
    echo "Native compilation..."
    CMAKE_OPTS=""
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake $CMAKE_OPTS ..
make -j$(nproc)

echo "Build complete: $BUILD_DIR/smarthub"
EOF
chmod +x ~/projects/smarthub/tools/build-app.sh
```

### 2.7.2 Deploy Script
```bash
cat > ~/projects/smarthub/tools/deploy.sh << 'EOF'
#!/bin/bash
set -e

TARGET_HOST="${1:-smarthub}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Deploying to $TARGET_HOST..."

# Stop running instance
ssh "$TARGET_HOST" "/etc/init.d/S90smarthub stop || true"

# Copy binary
scp "$PROJECT_DIR/app/build/smarthub" "$TARGET_HOST:/opt/smarthub/"

# Copy assets
rsync -av "$PROJECT_DIR/app/assets/" "$TARGET_HOST:/opt/smarthub/assets/"

# Copy config if needed
ssh "$TARGET_HOST" "mkdir -p /etc/smarthub"
scp "$PROJECT_DIR/app/assets/config.yaml" "$TARGET_HOST:/etc/smarthub/config.yaml"

# Start application
ssh "$TARGET_HOST" "/etc/init.d/S90smarthub start"

echo "Deployment complete"
EOF
chmod +x ~/projects/smarthub/tools/deploy.sh
```

---

## 2.8 Validation Checklist

Before proceeding to Phase 3, verify:

| Item | Status | Notes |
|------|--------|-------|
| Project compiles natively | ✅ | 11 test suites, 100+ tests passing |
| Project cross-compiles | ✅ | Via CI/CD (GitHub Actions) |
| Application starts on board | ✅ | Tested 2025-12-29: All services init, web server on :443 |
| LVGL displays on screen | ✅ | DRM backend with double buffering |
| Touch input works | ✅ | evdev input driver integrated |
| Web server responds on HTTP | ✅ | Mongoose server, 15 REST API tests |
| REST API returns device list | ✅ | /api/devices, /api/system/status work |
| MQTT client tested | ✅ | 13 unit tests, live broker tests available |
| RPMsg client tested | ✅ | 15 unit tests, hardware tests available |
| Database creates schema | ✅ | SQLite with all tables/indexes |
| Configuration loads from YAML | ✅ | yaml-cpp integration complete |
| Logging works | ✅ | Timestamped, leveled logging |
| Event bus delivers events | ✅ | Async queue + sync delivery |

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 2.1 Project Structure | 2 hours |
| 2.2 Core Infrastructure | 8-12 hours |
| 2.3 Database Layer | 4-6 hours |
| 2.4 LVGL Integration | 8-12 hours |
| 2.5 Web Server | 6-8 hours |
| 2.6 MQTT Client | 4-6 hours |
| 2.7 Build and Test | 4-6 hours |
| 2.8 Validation | 4 hours |
| **Total** | **40-56 hours** |
