cmake_minimum_required(VERSION 3.20)

project(TOPOΣ VERSION 1.0.0 LANGUAGES CXX)

find_package(nlohmann_json 3.11 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TOPOΣ_ENABLE_CURL "Enable optional live HTTP providers through libcurl" ON)
option(TOPOΣ_BUILD_TESTS "Build the test executable" ON)

add_library(topos_core
    src/analytics.cpp
    src/data.cpp
    src/dashboard.cpp
    src/util.cpp
)

target_include_directories(topos_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(topos_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(topos_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

if(TOPOΣ_ENABLE_CURL)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(topos_core PRIVATE CURL::libcurl)
        target_compile_definitions(topos_core PRIVATE TOPOS_WITH_CURL=1)
    endif()
endif()

add_executable(topos src/main.cpp)
target_link_libraries(topos PRIVATE topos_core)

if(TOPOΣ_BUILD_TESTS)
    enable_testing()
    add_executable(topos_tests tests/test_topos.cpp)
    target_link_libraries(topos_tests PRIVATE topos_core)
    add_test(NAME topos_tests COMMAND topos_tests)
endif()

install(TARGETS topos RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
