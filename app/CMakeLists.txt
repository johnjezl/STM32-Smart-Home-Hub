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
option(SMARTHUB_NATIVE_BUILD "Building natively (not cross-compiling)" OFF)

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

# SQLite3
find_package(SQLite3)
if(NOT SQLite3_FOUND)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
endif()

# OpenSSL
find_package(OpenSSL)
if(NOT OPENSSL_FOUND)
    pkg_check_modules(OPENSSL openssl)
endif()

# Mosquitto (MQTT library)
pkg_check_modules(MOSQUITTO libmosquitto)

# YAML-CPP (for configuration)
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    pkg_check_modules(YAMLCPP yaml-cpp)
endif()

# nlohmann_json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    # Try pkg-config
    pkg_check_modules(NLOHMANN_JSON nlohmann_json)
    if(NOT NLOHMANN_JSON_FOUND)
        message(STATUS "nlohmann_json not found via CMake or pkg-config, will use header-only")
        set(NLOHMANN_JSON_HEADER_ONLY ON)
    endif()
endif()

# LVGL (optional for now - display UI)
pkg_check_modules(LVGL lvgl)
if(NOT LVGL_FOUND)
    message(STATUS "LVGL not found - UI will be disabled")
    set(SMARTHUB_ENABLE_LVGL OFF)
else()
    set(SMARTHUB_ENABLE_LVGL ON)
endif()

# libdrm (required for LVGL DRM backend)
pkg_check_modules(LIBDRM libdrm)
if(SMARTHUB_ENABLE_LVGL AND NOT LIBDRM_FOUND)
    message(STATUS "libdrm not found - UI will be disabled")
    set(SMARTHUB_ENABLE_LVGL OFF)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(SQLite3_FOUND)
    include_directories(${SQLite3_INCLUDE_DIRS})
elseif(SQLITE3_FOUND)
    include_directories(${SQLITE3_INCLUDE_DIRS})
endif()

if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
elseif(OPENSSL_INCLUDE_DIRS)
    include_directories(${OPENSSL_INCLUDE_DIRS})
endif()

if(MOSQUITTO_FOUND)
    include_directories(${MOSQUITTO_INCLUDE_DIRS})
endif()

if(YAMLCPP_FOUND)
    include_directories(${YAMLCPP_INCLUDE_DIRS})
endif()

if(LVGL_FOUND)
    include_directories(${LVGL_INCLUDE_DIRS})
endif()

if(LIBDRM_FOUND)
    include_directories(${LIBDRM_INCLUDE_DIRS})
endif()

if(NLOHMANN_JSON_FOUND)
    include_directories(${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

# Source files - Core
set(CORE_SOURCES
    src/core/Logger.cpp
    src/core/Config.cpp
    src/core/EventBus.cpp
    src/core/Application.cpp
)

# Source files - Database
set(DATABASE_SOURCES
    src/database/Database.cpp
)

# Source files - Devices
set(DEVICES_SOURCES
    src/devices/Device.cpp
    src/devices/DeviceManager.cpp
    src/devices/DeviceTypeRegistry.cpp
    src/devices/Room.cpp
    src/devices/DeviceGroup.cpp
    src/devices/types/SwitchDevice.cpp
    src/devices/types/DimmerDevice.cpp
    src/devices/types/ColorLightDevice.cpp
    src/devices/types/TemperatureSensor.cpp
    src/devices/types/MotionSensor.cpp
)

# Source files - Protocols
set(PROTOCOLS_SOURCES
    src/protocols/ProtocolFactory.cpp
    src/protocols/mqtt/MqttClient.cpp
    src/protocols/zigbee/ZnpFrame.cpp
    src/protocols/zigbee/ZnpTransport.cpp
    src/protocols/zigbee/ZigbeeCoordinator.cpp
    src/protocols/zigbee/ZigbeeHandler.cpp
    src/protocols/zigbee/ZigbeeDeviceDatabase.cpp
)

# Source files - RPMsg
set(RPMSG_SOURCES
    src/rpmsg/RpmsgClient.cpp
)

# Source files - Web (using Mongoose)
set(WEB_SOURCES
    src/web/WebServer.cpp
)

# Add mongoose if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mongoose/mongoose.c")
    list(APPEND WEB_SOURCES third_party/mongoose/mongoose.c)
endif()

# Source files - UI (conditional on LVGL)
if(SMARTHUB_ENABLE_LVGL)
    set(UI_SOURCES
        src/ui/UIManager.cpp
    )
else()
    set(UI_SOURCES "")
endif()

# All sources
set(SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${DATABASE_SOURCES}
    ${DEVICES_SOURCES}
    ${PROTOCOLS_SOURCES}
    ${RPMSG_SOURCES}
    ${WEB_SOURCES}
    ${UI_SOURCES}
)

# Main executable
add_executable(smarthub ${SOURCES})

# Compile definitions
if(SMARTHUB_ENABLE_LVGL)
    target_compile_definitions(smarthub PRIVATE SMARTHUB_ENABLE_LVGL=1)
endif()

# Link libraries
target_link_libraries(smarthub PRIVATE Threads::Threads)

if(SQLite3_FOUND)
    target_link_libraries(smarthub PRIVATE SQLite::SQLite3)
elseif(SQLITE3_FOUND)
    target_link_libraries(smarthub PRIVATE ${SQLITE3_LIBRARIES})
endif()

if(OPENSSL_FOUND)
    target_link_libraries(smarthub PRIVATE OpenSSL::SSL OpenSSL::Crypto)
elseif(OPENSSL_LIBRARIES)
    target_link_libraries(smarthub PRIVATE ${OPENSSL_LIBRARIES})
endif()

if(MOSQUITTO_FOUND)
    target_link_libraries(smarthub PRIVATE ${MOSQUITTO_LIBRARIES})
endif()

# yaml-cpp is optional - Config.cpp has fallback INI parser
if(yaml-cpp_FOUND)
    target_link_libraries(smarthub PRIVATE yaml-cpp)
elseif(YAMLCPP_FOUND AND pkgcfg_lib_YAMLCPP_yaml-cpp)
    # Only link if the library was actually found (not just pkg-config metadata)
    target_link_libraries(smarthub PRIVATE ${YAMLCPP_LIBRARIES})
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(smarthub PRIVATE nlohmann_json::nlohmann_json)
endif()

if(SMARTHUB_ENABLE_LVGL AND LVGL_FOUND)
    target_link_libraries(smarthub PRIVATE ${LVGL_LIBRARIES})
    if(LIBDRM_FOUND)
        target_link_libraries(smarthub PRIVATE ${LIBDRM_LIBRARIES})
    endif()
endif()

# Dynamic loading for plugins (future)
target_link_libraries(smarthub PRIVATE ${CMAKE_DL_LIBS})

# Install
install(TARGETS smarthub DESTINATION /opt/smarthub/bin)
install(DIRECTORY assets/ DESTINATION /opt/smarthub/assets)

# Tests
if(SMARTHUB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Summary
message(STATUS "")
message(STATUS "SmartHub Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SQLite3: ${SQLite3_FOUND} OR ${SQLITE3_FOUND}")
message(STATUS "  OpenSSL: ${OPENSSL_FOUND}")
message(STATUS "  Mosquitto: ${MOSQUITTO_FOUND}")
message(STATUS "  YAML-CPP: ${yaml-cpp_FOUND} OR ${YAMLCPP_FOUND}")
message(STATUS "  nlohmann_json: ${nlohmann_json_FOUND}")
message(STATUS "  LVGL: ${SMARTHUB_ENABLE_LVGL}")
message(STATUS "  libdrm: ${LIBDRM_FOUND}")
message(STATUS "")
