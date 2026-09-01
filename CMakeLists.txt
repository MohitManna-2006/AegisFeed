cmake_minimum_required(VERSION 3.21)

project(
    AegisFeed
    VERSION 0.1.0
    DESCRIPTION "Deterministic Nasdaq TotalView-ITCH feed reconstruction"
    LANGUAGES CXX
)

include(CTest)
include(cmake/CompilerWarnings.cmake)

option(AEGIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(aegis_core STATIC src/common/core.cpp)
add_library(AegisFeed::core ALIAS aegis_core)

target_compile_features(aegis_core PUBLIC cxx_std_20)
target_include_directories(
    aegis_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
aegis_enable_compiler_warnings(aegis_core)

add_executable(aegis_replay src/app/replay_main.cpp)
target_link_libraries(aegis_replay PRIVATE AegisFeed::core)
aegis_enable_compiler_warnings(aegis_replay)

add_executable(aegis_feed src/app/feed_main.cpp)
target_link_libraries(aegis_feed PRIVATE AegisFeed::core)
aegis_enable_compiler_warnings(aegis_feed)

if(BUILD_TESTING)
    add_executable(aegis_smoke_test tests/unit/smoke_test.cpp)
    target_link_libraries(aegis_smoke_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_smoke_test)

    add_test(NAME aegis.smoke COMMAND aegis_smoke_test)
endif()
