cmake_minimum_required(VERSION 3.21)

project(
    AegisFeed
    VERSION 0.1.0
    DESCRIPTION "Deterministic Nasdaq TotalView-ITCH feed reconstruction"
    LANGUAGES CXX
)

include(CTest)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

option(AEGIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    aegis_core STATIC
    src/book/book_store.cpp
    src/book/order_book.cpp
    src/book/symbol_directory.cpp
    src/common/core.cpp
    src/itch/itch_decoder.cpp
)
add_library(AegisFeed::core ALIAS aegis_core)

target_compile_features(aegis_core PUBLIC cxx_std_20)
target_include_directories(
    aegis_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
aegis_enable_compiler_warnings(aegis_core)
aegis_enable_sanitizers(aegis_core)

add_executable(aegis_replay src/app/replay_main.cpp)
target_link_libraries(aegis_replay PRIVATE AegisFeed::core)
aegis_enable_compiler_warnings(aegis_replay)
aegis_enable_sanitizers(aegis_replay)

add_executable(aegis_feed src/app/feed_main.cpp)
target_link_libraries(aegis_feed PRIVATE AegisFeed::core)
aegis_enable_compiler_warnings(aegis_feed)
aegis_enable_sanitizers(aegis_feed)

if(BUILD_TESTING)
    add_executable(aegis_smoke_test tests/unit/smoke_test.cpp)
    target_link_libraries(aegis_smoke_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_smoke_test)
    aegis_enable_sanitizers(aegis_smoke_test)

    add_test(NAME aegis.smoke COMMAND aegis_smoke_test)

    add_executable(aegis_byte_reader_test tests/unit/byte_reader_test.cpp)
    target_link_libraries(aegis_byte_reader_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_byte_reader_test)
    aegis_enable_sanitizers(aegis_byte_reader_test)

    add_test(NAME aegis.byte_reader COMMAND aegis_byte_reader_test)

    add_executable(aegis_byte_writer_test tests/unit/byte_writer_test.cpp)
    target_link_libraries(aegis_byte_writer_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_byte_writer_test)
    aegis_enable_sanitizers(aegis_byte_writer_test)

    add_test(NAME aegis.byte_writer COMMAND aegis_byte_writer_test)

    add_executable(aegis_mold_session_test tests/unit/mold_session_test.cpp)
    target_link_libraries(aegis_mold_session_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_mold_session_test)
    aegis_enable_sanitizers(aegis_mold_session_test)

    add_test(NAME aegis.mold_session COMMAND aegis_mold_session_test)

    add_executable(aegis_unique_fd_test tests/unit/unique_fd_test.cpp)
    target_link_libraries(aegis_unique_fd_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_unique_fd_test)
    aegis_enable_sanitizers(aegis_unique_fd_test)

    add_test(NAME aegis.unique_fd COMMAND aegis_unique_fd_test)

    add_executable(aegis_itch_types_test tests/unit/itch_types_test.cpp)
    target_link_libraries(aegis_itch_types_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_itch_types_test)
    aegis_enable_sanitizers(aegis_itch_types_test)

    add_test(NAME aegis.itch_types COMMAND aegis_itch_types_test)

    add_executable(aegis_itch_decoder_test tests/unit/itch_decoder_test.cpp)
    target_link_libraries(aegis_itch_decoder_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_itch_decoder_test)
    aegis_enable_sanitizers(aegis_itch_decoder_test)

    add_test(NAME aegis.itch_decoder COMMAND aegis_itch_decoder_test)

    add_executable(aegis_order_book_test tests/unit/order_book_test.cpp)
    target_link_libraries(aegis_order_book_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_order_book_test)
    aegis_enable_sanitizers(aegis_order_book_test)

    add_test(NAME aegis.order_book COMMAND aegis_order_book_test)

    add_executable(
        aegis_symbol_directory_test
        tests/unit/symbol_directory_test.cpp
    )
    target_link_libraries(aegis_symbol_directory_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_symbol_directory_test)
    aegis_enable_sanitizers(aegis_symbol_directory_test)

    add_test(NAME aegis.symbol_directory COMMAND aegis_symbol_directory_test)

    add_executable(aegis_book_store_test tests/unit/book_store_test.cpp)
    target_link_libraries(aegis_book_store_test PRIVATE AegisFeed::core)
    aegis_enable_compiler_warnings(aegis_book_store_test)
    aegis_enable_sanitizers(aegis_book_store_test)

    add_test(NAME aegis.book_store COMMAND aegis_book_store_test)
endif()
