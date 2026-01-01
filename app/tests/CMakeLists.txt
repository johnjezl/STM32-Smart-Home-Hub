# SmartHub Test Configuration
#
# Uses Google Test for unit and integration testing

cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Try to fetch GTest via FetchContent (modern CMake approach)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Prevent GTest from overriding our compiler/linker options
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(GTEST_AVAILABLE ON)
else()
    set(GTEST_AVAILABLE ON)
endif()

if(GTEST_AVAILABLE)
    message(STATUS "Google Test found - building tests")

    # Include directories for tests
    include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Test library sources (non-main sources for testing)
    set(TESTABLE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Config.cpp
        ${CMAKE_SOURCE_DIR}/src/core/EventBus.cpp
        ${CMAKE_SOURCE_DIR}/src/database/Database.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/Device.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/DeviceManager.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/DeviceTypeRegistry.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/Room.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/DeviceGroup.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/types/SwitchDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/types/DimmerDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/types/ColorLightDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/types/TemperatureSensor.cpp
        ${CMAKE_SOURCE_DIR}/src/devices/types/MotionSensor.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/ProtocolFactory.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/mqtt/MqttClient.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/zigbee/ZnpFrame.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/zigbee/ZnpTransport.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/zigbee/ZigbeeCoordinator.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/zigbee/ZigbeeHandler.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/zigbee/ZigbeeDeviceDatabase.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/http/HttpClient.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/wifi/MqttDiscovery.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/wifi/ShellyDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/wifi/TuyaDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/protocols/wifi/WifiHandler.cpp
        ${CMAKE_SOURCE_DIR}/src/rpmsg/RpmsgClient.cpp
        ${CMAKE_SOURCE_DIR}/src/security/CertManager.cpp
        ${CMAKE_SOURCE_DIR}/src/security/UserManager.cpp
        ${CMAKE_SOURCE_DIR}/src/security/SessionManager.cpp
        ${CMAKE_SOURCE_DIR}/src/security/ApiTokenManager.cpp
        ${CMAKE_SOURCE_DIR}/src/security/CredentialStore.cpp
        ${CMAKE_SOURCE_DIR}/src/security/SetupManager.cpp
        ${CMAKE_SOURCE_DIR}/src/web/WebServer.cpp
    )

    # Mongoose if available
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/mongoose/mongoose.c")
        list(APPEND TESTABLE_SOURCES ${CMAKE_SOURCE_DIR}/third_party/mongoose/mongoose.c)
    endif()

    # Create a library from testable sources
    add_library(smarthub_lib STATIC ${TESTABLE_SOURCES})
    target_include_directories(smarthub_lib PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_link_libraries(smarthub_lib PUBLIC Threads::Threads)

    # Link external libraries to test library
    if(SQLite3_FOUND)
        target_link_libraries(smarthub_lib PUBLIC SQLite::SQLite3)
    elseif(SQLITE3_FOUND)
        target_link_libraries(smarthub_lib PUBLIC ${SQLITE3_LIBRARIES})
    endif()

    if(OPENSSL_FOUND)
        target_link_libraries(smarthub_lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    elseif(OPENSSL_LIBRARIES)
        target_link_libraries(smarthub_lib PUBLIC ${OPENSSL_LIBRARIES})
    endif()

    if(MOSQUITTO_FOUND)
        target_link_libraries(smarthub_lib PUBLIC ${MOSQUITTO_LIBRARIES})
    endif()

    if(yaml-cpp_FOUND)
        target_link_libraries(smarthub_lib PUBLIC yaml-cpp)
    elseif(YAMLCPP_FOUND)
        target_link_libraries(smarthub_lib PUBLIC ${YAMLCPP_LIBRARIES})
    endif()

    if(nlohmann_json_FOUND)
        target_link_libraries(smarthub_lib PUBLIC nlohmann_json::nlohmann_json)
    endif()

    # ==== Unit Tests ====

    # Core tests
    add_executable(test_logger core/test_logger.cpp)
    target_link_libraries(test_logger PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Logger COMMAND test_logger)

    add_executable(test_config core/test_config.cpp)
    target_link_libraries(test_config PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Config COMMAND test_config)

    add_executable(test_eventbus core/test_eventbus.cpp)
    target_link_libraries(test_eventbus PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME EventBus COMMAND test_eventbus)

    # Database tests
    add_executable(test_database database/test_database.cpp)
    target_link_libraries(test_database PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Database COMMAND test_database)

    # Device tests
    add_executable(test_device devices/test_device.cpp)
    target_link_libraries(test_device PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Device COMMAND test_device)

    add_executable(test_device_manager devices/test_device_manager.cpp)
    target_link_libraries(test_device_manager PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME DeviceManager COMMAND test_device_manager)

    add_executable(test_organization devices/test_organization.cpp)
    target_link_libraries(test_organization PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Organization COMMAND test_organization)

    # Web tests
    add_executable(test_webserver web/test_webserver.cpp)
    target_link_libraries(test_webserver PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME WebServer COMMAND test_webserver)

    # Security tests
    add_executable(test_security security/test_security.cpp)
    target_link_libraries(test_security PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Security COMMAND test_security)

    # Protocol tests
    add_executable(test_mqtt protocols/test_mqtt.cpp)
    target_link_libraries(test_mqtt PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME MQTT COMMAND test_mqtt)

    add_executable(test_protocol_factory protocols/test_protocol_factory.cpp)
    target_link_libraries(test_protocol_factory PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME ProtocolFactory COMMAND test_protocol_factory)

    # Zigbee tests
    add_executable(test_zigbee protocols/test_zigbee.cpp)
    target_link_libraries(test_zigbee PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Zigbee COMMAND test_zigbee)

    # WiFi Protocol tests
    add_executable(test_wifi protocols/test_wifi.cpp)
    target_link_libraries(test_wifi PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME WiFi COMMAND test_wifi)

    # RPMsg tests
    add_executable(test_rpmsg rpmsg/test_rpmsg.cpp)
    target_link_libraries(test_rpmsg PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME RPMsg COMMAND test_rpmsg)

    # UI tests (conditional on LVGL availability)
    if(SMARTHUB_ENABLE_LVGL)
        add_executable(test_ui ui/test_ui.cpp ${CMAKE_SOURCE_DIR}/src/ui/UIManager.cpp)
        target_link_libraries(test_ui PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
        target_link_libraries(test_ui PRIVATE ${LVGL_LIBRARIES})
        if(LIBDRM_FOUND)
            target_link_libraries(test_ui PRIVATE ${LIBDRM_LIBRARIES})
        endif()
        target_compile_definitions(test_ui PRIVATE SMARTHUB_ENABLE_LVGL=1)
        add_test(NAME UI COMMAND test_ui)
    else()
        # Stub test when LVGL not available
        add_executable(test_ui ui/test_ui.cpp)
        target_link_libraries(test_ui PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
        add_test(NAME UI COMMAND test_ui)
    endif()

    # ==== Integration Tests ====

    add_executable(test_integration integration/test_integration.cpp)
    target_link_libraries(test_integration PRIVATE smarthub_lib GTest::gtest GTest::gtest_main)
    add_test(NAME Integration COMMAND test_integration)

    # ==== All-in-one test runner ====

    set(ALL_TEST_SOURCES
        main.cpp
        core/test_logger.cpp
        core/test_config.cpp
        core/test_eventbus.cpp
        database/test_database.cpp
        devices/test_device.cpp
        devices/test_device_manager.cpp
        devices/test_organization.cpp
        web/test_webserver.cpp
        security/test_security.cpp
        protocols/test_mqtt.cpp
        protocols/test_protocol_factory.cpp
        protocols/test_zigbee.cpp
        protocols/test_wifi.cpp
        rpmsg/test_rpmsg.cpp
        integration/test_integration.cpp
        ui/test_ui.cpp
    )

    add_executable(smarthub_tests ${ALL_TEST_SOURCES})
    target_link_libraries(smarthub_tests PRIVATE smarthub_lib GTest::gtest)

    # Add UI dependencies if LVGL is available
    if(SMARTHUB_ENABLE_LVGL)
        target_sources(smarthub_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/ui/UIManager.cpp)
        target_link_libraries(smarthub_tests PRIVATE ${LVGL_LIBRARIES})
        if(LIBDRM_FOUND)
            target_link_libraries(smarthub_tests PRIVATE ${LIBDRM_LIBRARIES})
        endif()
        target_compile_definitions(smarthub_tests PRIVATE SMARTHUB_ENABLE_LVGL=1)
    endif()

    add_test(NAME AllTests COMMAND smarthub_tests)

    # ==== Code Coverage ====

    # Note: SMARTHUB_COVERAGE option is defined in the parent CMakeLists.txt
    if(SMARTHUB_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Use PUBLIC for link options so coverage links propagate to test executables
        target_compile_options(smarthub_lib PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(smarthub_lib PUBLIC --coverage)
        target_compile_options(smarthub_tests PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(smarthub_tests PRIVATE --coverage)

        # Coverage target
        find_program(LCOV lcov)
        find_program(GENHTML genhtml)
        if(LCOV AND GENHTML)
            add_custom_target(coverage
                COMMAND ${LCOV} --capture --directory . --output-file coverage.info
                COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/third_party/*' '*/tests/*' --output-file coverage.info
                COMMAND ${GENHTML} coverage.info --output-directory coverage_html
                COMMAND echo "Coverage report generated in coverage_html/"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                DEPENDS smarthub_tests
            )
        endif()
    endif()

else()
    message(WARNING "Google Test not found - tests will not be built")
endif()
