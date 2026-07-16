cmake_minimum_required(VERSION 3.20)

project(serdes_firmware_lab VERSION 0.1.0 LANGUAGES CXX)

option(SERDES_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(serdes_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

add_library(serdes_firmware_core
    src/firmware_controller.cpp
    src/phy_driver.cpp
    src/prbs31.cpp
    src/statistics.cpp
)

target_include_directories(serdes_firmware_core PUBLIC include)
target_compile_features(serdes_firmware_core PUBLIC cxx_std_20)
serdes_enable_warnings(serdes_firmware_core)

add_library(serdes_behavioral_model
    src/channel_profiles.cpp
    src/simulated_phy.cpp
)
target_link_libraries(serdes_behavioral_model PUBLIC serdes_firmware_core)
serdes_enable_warnings(serdes_behavioral_model)

if(SERDES_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(
        serdes_firmware_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(serdes_firmware_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(serdes_lab app/main.cpp)
target_link_libraries(serdes_lab PRIVATE serdes_behavioral_model)
serdes_enable_warnings(serdes_lab)

include(CTest)
if(BUILD_TESTING)
    add_executable(serdes_tests tests/serdes_tests.cpp)
    target_link_libraries(serdes_tests PRIVATE serdes_behavioral_model)
    serdes_enable_warnings(serdes_tests)
    add_test(NAME serdes_cpp_tests COMMAND serdes_tests)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME serdes_python_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                    -s ${CMAKE_CURRENT_SOURCE_DIR}/python/tests
                    -p test_*.py
        )
        add_test(
            NAME serdes_regression_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/python/run_regression.py
                    --executable $<TARGET_FILE:serdes_lab>
                    --seeds 2
                    --verify-symbols 50000
                    --output ${CMAKE_CURRENT_BINARY_DIR}/regression-smoke
        )
    endif()
endif()
