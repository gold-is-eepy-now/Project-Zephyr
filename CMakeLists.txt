cmake_minimum_required(VERSION 3.16)
project(ZephyrBrowser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZEPHYR_ENABLE_CURL "Use libcurl for HTTP/HTTPS transport" ON)

add_library(zephyr_core
    browser_core.cpp
    dom.cpp
    css.cpp
)

target_include_directories(zephyr_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ZEPHYR_ENABLE_CURL)
    find_package(CURL)
    if(CURL_FOUND)
        target_link_libraries(zephyr_core PRIVATE CURL::libcurl)
        target_compile_definitions(zephyr_core PRIVATE ZEPHYR_USE_CURL=1)
    endif()
endif()

add_executable(zephyr_cli browser.cpp)
target_link_libraries(zephyr_cli PRIVATE zephyr_core)

if(WIN32)
    add_executable(zephyr_gui WIN32 gui_browser.cpp)
    target_link_libraries(zephyr_gui PRIVATE zephyr_core ws2_32 gdi32)
endif()

enable_testing()
add_executable(zephyr_core_tests core_tests.cpp)
target_link_libraries(zephyr_core_tests PRIVATE zephyr_core)
add_test(NAME zephyr_core_tests COMMAND zephyr_core_tests)
