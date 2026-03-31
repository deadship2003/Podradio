# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                    PODRADIO CMake Build System                            ║
# ║                   Terminal Podcast/Radio Player                           ║
# ║                 Author: Panic <Deadship2003@gmail.com>                    ║
# ╚═══════════════════════════════════════════════════════════════════════════╝
#
# Multi-module build: src/*.cpp → podradio_lib (static library) + podradio (executable)
# Cross-platform support: Windows / Linux / macOS
# Testing: BUILD_TESTS=ON enables CTest + GTest integration

cmake_minimum_required(VERSION 3.16)

project(PodRadio
    VERSION 0.5.0
    DESCRIPTION "Terminal Podcast/Radio Player"
    LANGUAGES CXX
)

# Version info
set(PODRADIO_VERSION_SUFFIX "B9n3e5g3n")
set(PODRADIO_FULL_VERSION "${PROJECT_VERSION}-${PODRADIO_VERSION_SUFFIX}")

# =============================================================================
# Build options
# =============================================================================
option(BUILD_TESTS "Build unit tests (requires GTest)" OFF)

# =============================================================================
# C++ standard
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build type
# =============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "PodRadio version: ${PODRADIO_FULL_VERSION}")
message(STATUS "Build tests: ${BUILD_TESTS}")

# =============================================================================
# Platform detection
# =============================================================================
if(WIN32)
    set(PODRADIO_PLATFORM "Windows")
    add_definitions(-DPODRADIO_WINDOWS)
elseif(APPLE)
    set(PODRADIO_PLATFORM "macOS")
    add_definitions(-DPODRADIO_MACOS)
elseif(UNIX)
    set(PODRADIO_PLATFORM "Linux")
    add_definitions(-DPODRADIO_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Target platform: ${PODRADIO_PLATFORM}")

# =============================================================================
# Source file lists
# =============================================================================
# All module source files (excluding main.cpp for library, excluding legacy monolith)
set(PODRADIO_SOURCES
    src/app.cpp
    src/cache_manager.cpp
    src/config.cpp
    src/database.cpp
    src/event_log.cpp
    src/itunes_search.cpp
    src/layout.cpp
    src/logger.cpp
    src/mpv_controller.cpp
    src/network.cpp
    src/online_state.cpp
    src/parsers.cpp
    src/persistence.cpp
    src/progress.cpp
    src/sleep_timer.cpp
    src/statusbar_color.cpp
    src/terminal_state.cpp
    src/ui.cpp
    src/utils.cpp
    src/xml_helpers.cpp
    src/youtube_cache.cpp
)

set(PODRADIO_HEADERS
    include/podradio/app.h
    include/podradio/cache_manager.h
    include/podradio/common.h
    include/podradio/config.h
    include/podradio/database.h
    include/podradio/event_log.h
    include/podradio/icon_manager.h
    include/podradio/itunes_search.h
    include/podradio/layout.h
    include/podradio/logger.h
    include/podradio/mpv_controller.h
    include/podradio/network.h
    include/podradio/online_state.h
    include/podradio/parsers.h
    include/podradio/persistence.h
    include/podradio/progress.h
    include/podradio/sleep_timer.h
    include/podradio/statusbar_color.h
    include/podradio/url_classifier.h
    include/podradio/utils.h
)

# =============================================================================
# Find dependencies
# =============================================================================
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

# MPV
if(PkgConfig_FOUND)
    pkg_check_modules(MPV IMPORTED_TARGET mpv QUIET)
endif()
if(NOT MPV_FOUND)
    find_library(MPV_LIBRARY NAMES mpv)
    if(MPV_LIBRARY)
        set(MPV_FOUND TRUE)
    endif()
endif()

# SQLite3 - try multiple methods for better cross-platform support
# Method 1: PkgConfig (most reliable on Linux)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3_PKG IMPORTED_TARGET sqlite3 QUIET)
endif()
# Method 2: CMake config (may use different target names)
if(NOT SQLITE3_PKG_FOUND)
    find_package(SQLite3 QUIET)
endif()
# Method 3: Fallback to find_library
if(NOT SQLITE3_PKG_FOUND AND NOT SQLite3_FOUND)
    find_library(SQLITE3_LIBRARY NAMES sqlite3)
    if(SQLITE3_LIBRARY)
        set(SQLite3_FOUND TRUE)
        set(SQLITE3_MANUAL TRUE)
    endif()
endif()

# CURL
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    find_library(CURL_LIBRARY NAMES curl)
    if(CURL_LIBRARY)
        set(CURL_FOUND TRUE)
    endif()
endif()

# libxml2
find_package(LibXml2 QUIET)
if(NOT LibXml2_FOUND)
    find_library(XML2_LIBRARY NAMES xml2)
    if(XML2_LIBRARY)
        set(LibXml2_FOUND TRUE)
    endif()
endif()

# fmt
find_package(fmt QUIET)

# nlohmann_json
find_package(nlohmann_json QUIET)

# ncurses (Linux/macOS)
# CRITICAL: Must use ncursesw (wide character version) for CJK/Emoji support.
# The narrow ncurses library cannot properly handle multi-byte UTF-8 characters.
# Priority: pkg-config ncursesw > find_library ncursesw > Curses module
if(UNIX)
    # Method 1: pkg-config for ncursesw (most reliable, ensures wide version)
    if(PkgConfig_FOUND)
        pkg_check_modules(NCURSESW_PKG IMPORTED_TARGET ncursesw QUIET)
    endif()
    # Method 2: find_library specifically for ncursesw
    if(NOT NCURSESW_PKG_FOUND)
        find_library(NCURSES_LIBRARY NAMES ncursesw)
        find_path(NCURSES_INCLUDE_DIR ncursesw/curses.h)
        if(NCURSES_LIBRARY)
            set(NCURSESW_FOUND TRUE)
        endif()
    endif()
    # Method 3: CMake Curses module (may find narrow version as fallback)
    if(NOT NCURSESW_PKG_FOUND AND NOT NCURSESW_FOUND)
        find_package(Curses QUIET)
        if(CURSES_FOUND)
            # Check if it found ncursesw (look for ncursesw/curses.h in include dirs)
            if(CURSES_INCLUDE_DIR AND EXISTS "${CURSES_INCLUDE_DIR}/ncursesw/curses.h")
                message(STATUS "ncurses: Curses module found wide version")
            else()
                message(WARNING "ncurses: Found narrow version only - CJK/Emoji may not display correctly. Install libncursesw-dev for wide character support.")
            endif()
        endif()
    endif()
    # Set CURSES_FOUND and CURSES_LIBRARIES for linking
    if(NCURSESW_PKG_FOUND)
        set(CURSES_FOUND TRUE)
        set(CURSES_LIBRARIES PkgConfig::NCURSESW_PKG)
    elseif(NCURSESW_FOUND)
        set(CURSES_FOUND TRUE)
        set(CURSES_LIBRARIES ${NCURSES_LIBRARY})
    endif()
endif()

# PDCurses (Windows ncurses replacement)
if(WIN32)
    find_package(PDCurses QUIET)
    if(NOT PDCurses_FOUND)
        find_library(PDCURSES_LIBRARY NAMES pdcurses curses)
        find_path(PDCURSES_INCLUDE_DIR curses.h)
        if(PDCURSES_LIBRARY)
            set(PDCURSES_FOUND TRUE)
            message(STATUS "PDCurses: Found (${PDCURSES_LIBRARY})")
        else()
            message(WARNING "PDCurses not found - TUI disabled on Windows")
        endif()
    endif()
endif()

# =============================================================================
# Compiler options
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -g)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
endif()

# MinGW: static linking for Windows
if(MINGW)
    add_compile_options(-static -static-libgcc -static-libstdc++)
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# =============================================================================
# Static library target: podradio_lib (all modules except main.cpp)
# This allows unit tests to link against the library without pulling in main()
# =============================================================================
add_library(podradio_lib STATIC
    ${PODRADIO_SOURCES}
    ${PODRADIO_HEADERS}
)

target_include_directories(podradio_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies to library
target_link_libraries(podradio_lib PUBLIC
    Threads::Threads
)

# MPV
if(MPV_FOUND)
    if(TARGET PkgConfig::MPV)
        target_link_libraries(podradio_lib PUBLIC PkgConfig::MPV)
    elseif(MPV_LIBRARY)
        target_link_libraries(podradio_lib PUBLIC ${MPV_LIBRARY})
    endif()
    message(STATUS "MPV: Found")
else()
    message(WARNING "MPV not found - media playback disabled")
endif()

# SQLite3
if(SQLITE3_PKG_FOUND)
    target_link_libraries(podradio_lib PUBLIC PkgConfig::SQLITE3_PKG)
    message(STATUS "SQLite3: Found via PkgConfig")
elseif(TARGET SQLite3::SQLite3)
    target_link_libraries(podradio_lib PUBLIC SQLite3::SQLite3)
    message(STATUS "SQLite3: Found (SQLite3::SQLite3)")
elseif(TARGET SQLite::SQLite3)
    target_link_libraries(podradio_lib PUBLIC SQLite::SQLite3)
    message(STATUS "SQLite3: Found (SQLite::SQLite3)")
elseif(SQLITE3_MANUAL)
    target_link_libraries(podradio_lib PUBLIC ${SQLITE3_LIBRARY})
    message(STATUS "SQLite3: Found (${SQLITE3_LIBRARY})")
elseif(SQLite3_FOUND)
    if(DEFINED SQLite3_LIBRARIES)
        target_link_libraries(podradio_lib PUBLIC ${SQLite3_LIBRARIES})
        message(STATUS "SQLite3: Found (${SQLite3_LIBRARIES})")
    else()
        message(WARNING "SQLite3 found but no library to link")
    endif()
else()
    message(WARNING "SQLite3 not found - database features disabled")
endif()

# CURL
if(CURL_FOUND)
    if(TARGET CURL::libcurl)
        target_link_libraries(podradio_lib PUBLIC CURL::libcurl)
    elseif(CURL_LIBRARY)
        target_link_libraries(podradio_lib PUBLIC ${CURL_LIBRARY})
    endif()
    message(STATUS "CURL: Found")
else()
    message(WARNING "CURL not found - network features disabled")
endif()

# libxml2
if(LibXml2_FOUND)
    if(TARGET LibXml2::LibXml2)
        target_link_libraries(podradio_lib PUBLIC LibXml2::LibXml2)
    elseif(XML2_LIBRARY)
        target_link_libraries(podradio_lib PUBLIC ${XML2_LIBRARY})
    endif()
    message(STATUS "libxml2: Found")
else()
    message(WARNING "libxml2 not found - RSS parsing disabled")
endif()

# fmt
if(fmt_FOUND)
    target_link_libraries(podradio_lib PUBLIC fmt::fmt)
    message(STATUS "fmt: Found")
else()
    message(STATUS "fmt: Using bundled version")
endif()

# nlohmann_json
if(nlohmann_json_FOUND)
    target_link_libraries(podradio_lib PUBLIC nlohmann_json::nlohmann_json)
    message(STATUS "nlohmann_json: Found")
else()
    message(STATUS "nlohmann_json: Using bundled version")
endif()

# ncurses (link)
if(UNIX)
    if(CURSES_FOUND)
        if(NCURSESW_PKG_FOUND)
            target_link_libraries(podradio_lib PUBLIC PkgConfig::NCURSESW_PKG)
            message(STATUS "ncurses: Linked ncursesw (wide char support)")
        else()
            target_link_libraries(podradio_lib PUBLIC ${CURSES_LIBRARIES})
            if(NCURSES_INCLUDE_DIR)
                target_include_directories(podradio_lib PUBLIC ${NCURSES_INCLUDE_DIR})
            endif()
            message(STATUS "ncurses: Linked (${CURSES_LIBRARIES})")
        endif()
    else()
        message(WARNING "ncurses not found - TUI disabled")
    endif()
endif()

# PDCurses (link) - Windows only
if(WIN32 AND PDCURSES_FOUND)
    target_link_libraries(podradio_lib PUBLIC ${PDCURSES_LIBRARY})
    if(PDCURSES_INCLUDE_DIR)
        target_include_directories(podradio_lib PUBLIC ${PDCURSES_INCLUDE_DIR})
    endif()
    message(STATUS "PDCurses: Linked (${PDCURSES_LIBRARY})")
endif()

# Windows: Winsock for networking (required by libcurl)
if(WIN32)
    target_link_libraries(podradio_lib PUBLIC ws2_32)
endif()

# =============================================================================
# Executable target: podradio (main.cpp + library)
# =============================================================================
add_executable(podradio
    src/main.cpp
)

target_link_libraries(podradio PRIVATE podradio_lib)

# =============================================================================
# Testing
# =============================================================================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS podradio
    RUNTIME DESTINATION bin
)

install(FILES README.md LICENSE
    DESTINATION share/doc/podradio
)

# Install man page
if(UNIX)
    install(FILES man/podradio.1
        DESTINATION share/man/man1
    )
endif()

# =============================================================================
# Packaging (CPack)
# =============================================================================
set(CPACK_PACKAGE_NAME "PodRadio")
set(CPACK_PACKAGE_VERSION ${PODRADIO_FULL_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Terminal Podcast/Radio Player")
set(CPACK_PACKAGE_VENDOR "Panic")
set(CPACK_PACKAGE_CONTACT "Deadship2003@gmail.com")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "mpv,libsqlite3-0,libcurl4,libxml2,libncursesw6")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)

# =============================================================================
# Configuration summary
# =============================================================================
message(STATUS "")
message(STATUS "=====================================================================")
message(STATUS "  PodRadio ${PODRADIO_FULL_VERSION} Configuration Summary")
message(STATUS "=====================================================================")
message(STATUS "  Platform:     ${PODRADIO_PLATFORM}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests:        ${BUILD_TESTS}")
message(STATUS "  Sources:      ${CMAKE_CURRENT_SOURCE_DIR}/src/ (${CMAKE_CURRENT_SOURCE_DIR}/main.cpp + ${PODRADIO_LIB} library)")
message(STATUS "")
message(STATUS "  Dependencies:")
message(STATUS "    MPV:       ${MPV_FOUND}")
message(STATUS "    SQLite3:   ${SQLite3_FOUND} (pkg=${SQLITE3_PKG_FOUND})")
message(STATUS "    CURL:      ${CURL_FOUND}")
message(STATUS "    libxml2:   ${LibXml2_FOUND}")
message(STATUS "    ncurses:   ${CURSES_FOUND} (wide=${NCURSESW_PKG_FOUND})")
message(STATUS "    fmt:       ${fmt_FOUND}")
message(STATUS "    json:      ${nlohmann_json_FOUND}")
message(STATUS "=====================================================================")
message(STATUS "")
