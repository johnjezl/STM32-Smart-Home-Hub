cmake_minimum_required(VERSION 3.16)

# Must be set before project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Toolchain settings - MUST be before project()
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "objcopy tool")
set(CMAKE_SIZE arm-none-eabi-size CACHE FILEPATH "size tool")

project(smarthub-m4 VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# CPU flags for Cortex-M4 with FPU
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

# Compiler flags
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -Wextra -ffunction-sections -fdata-sections -Os")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -x assembler-with-cpp")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "-g3 -Og")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -Og")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")

# Linker flags
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32mp15xx_m4.ld)
set(CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,-Map=smarthub-m4.map --specs=nosys.specs --specs=nano.specs -nostartfiles")

# Source files
set(SOURCES
    # Main application
    src/main.cpp

    # System
    src/system/startup.cpp
    src/system/clock.cpp

    # Drivers
    src/drivers/gpio.cpp
    src/drivers/i2c.cpp

    # RPMsg
    src/rpmsg/resource_table.c
    src/rpmsg/rpmsg.cpp

    # Sensors
    src/sensors/sht31.cpp
    src/sensors/sensor_manager.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Create ELF executable
add_executable(smarthub-m4.elf ${SOURCES})

# Generate binary and hex files
add_custom_command(TARGET smarthub-m4.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary smarthub-m4.elf smarthub-m4.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex smarthub-m4.elf smarthub-m4.hex
    COMMAND ${CMAKE_SIZE} smarthub-m4.elf
    COMMENT "Generating binary and hex files"
)

# Install target
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/smarthub-m4.elf
    ${CMAKE_CURRENT_BINARY_DIR}/smarthub-m4.bin
    DESTINATION firmware
)
