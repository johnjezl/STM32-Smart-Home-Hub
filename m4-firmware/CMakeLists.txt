cmake_minimum_required(VERSION 3.16)

# Must be set before project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

project(smarthub-m4 VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Toolchain settings (can be overridden by toolchain file)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# CPU flags for Cortex-M4 with FPU
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

# Compiler flags
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -Wextra -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")

# Linker flags
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32mp15xx_m4.ld)
set(CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,-Map=smarthub-m4.map --specs=nosys.specs")

# Source files
set(SOURCES
    src/main.cpp
    # src/drivers/gpio.cpp
    # src/drivers/i2c.cpp
    # src/drivers/uart.cpp
    # src/sensors/sensor_manager.cpp
    # src/rpmsg/rpmsg_handler.cpp
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
