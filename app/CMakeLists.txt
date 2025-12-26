cmake_minimum_required(VERSION 3.16)
project(smarthub-app VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
# find_package(SQLite3 REQUIRED)
# find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    # Core module
    # src/core/event_bus.cpp
    # src/core/config.cpp
    # src/core/logger.cpp
    # Database module
    # src/database/database.cpp
    # Device module
    # src/devices/device_manager.cpp
    # UI module
    # src/ui/ui_manager.cpp
    # Web module
    # src/web/web_server.cpp
    # RPMsg module
    # src/rpmsg/rpmsg_client.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Create executable
add_executable(smarthub ${SOURCES})

# Link libraries
target_link_libraries(smarthub
    # SQLite3::SQLite3
    # Threads::Threads
    # lvgl
    # mongoose
    # mosquitto
)

# Installation
install(TARGETS smarthub DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/smarthub/assets)
