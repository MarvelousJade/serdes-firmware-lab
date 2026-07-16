cmake_minimum_required(VERSION 3.20)

project(serdes_firmware_lab VERSION 0.1.0 LANGUAGES CXX)

option(SERDES_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(serdes_fw
    src/channel_profiles.cpp
    src/firmware_controller.cpp
    src/phy_driver.cpp
    src/prbs31.cpp
    src/simulated_phy.cpp
)

target_include_directories(serdes_fw PUBLIC include)
target_compile_features(serdes_fw PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(serdes_fw PRIVATE /W4 /permissive-)
else()
    target_compile_options(serdes_fw PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(SERDES_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(serdes_fw PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(serdes_fw PUBLIC -fsanitize=address,undefined)
endif()

add_executable(serdes_lab app/main.cpp)
target_link_libraries(serdes_lab PRIVATE serdes_fw)

include(CTest)
if(BUILD_TESTING)
    add_executable(serdes_tests tests/serdes_tests.cpp)
    target_link_libraries(serdes_tests PRIVATE serdes_fw)
    add_test(NAME serdes_cpp_tests COMMAND serdes_tests)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME serdes_python_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                    -s ${CMAKE_CURRENT_SOURCE_DIR}/python/tests
                    -p test_*.py
        )
    endif()
endif()

