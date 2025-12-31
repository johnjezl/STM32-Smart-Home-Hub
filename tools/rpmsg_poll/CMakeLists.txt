cmake_minimum_required(VERSION 3.14)
project(rpmsg_poll_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch GTest if not installed
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(test_rpmsg_poll test_rpmsg_poll.cpp)

target_link_libraries(test_rpmsg_poll
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

# Enable testing
enable_testing()
add_test(NAME RpmsgPollTests COMMAND test_rpmsg_poll)
