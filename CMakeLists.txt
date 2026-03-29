# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                    PODRADIO CMake Build System                            ║
# ║                   Terminal Podcast/Radio Player                           ║
# ║                 Author: Panic <Deadship2003@gmail.com>                    ║
# ╚═══════════════════════════════════════════════════════════════════════════╝
#
# 完整功能版本 - 包含所有12,934行代码
# 跨平台支持: Windows / Linux / macOS

cmake_minimum_required(VERSION 3.16)

project(PodRadio
    VERSION 0.5.0
    DESCRIPTION "Terminal Podcast/Radio Player"
    LANGUAGES CXX
)

# 版本信息
set(PODRADIO_VERSION_SUFFIX "B9n3e5g3n")
set(PODRADIO_FULL_VERSION "${PROJECT_VERSION}-${PODRADIO_VERSION_SUFFIX}")

# =============================================================================
# C++ 标准设置
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出编译命令
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# 构建类型
# =============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "PodRadio version: ${PODRADIO_FULL_VERSION}")

# =============================================================================
# 平台检测
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
# 查找依赖库
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
if(UNIX)
    find_package(Curses QUIET)
    if(NOT CURSES_FOUND)
        find_library(NCURSES_LIBRARY NAMES ncurses ncursesw)
        if(NCURSES_LIBRARY)
            set(CURSES_FOUND TRUE)
            set(CURSES_LIBRARIES ${NCURSES_LIBRARY})
        endif()
    endif()
endif()

# =============================================================================
# 编译器选项
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

# =============================================================================
# 可执行文件
# =============================================================================
add_executable(podradio
    src/podradio.cpp
)

target_include_directories(podradio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# =============================================================================
# 链接库
# =============================================================================
target_link_libraries(podradio PRIVATE
    Threads::Threads
)

# MPV
if(MPV_FOUND)
    if(TARGET PkgConfig::MPV)
        target_link_libraries(podradio PRIVATE PkgConfig::MPV)
    elseif(MPV_LIBRARY)
        target_link_libraries(podradio PRIVATE ${MPV_LIBRARY})
    endif()
    message(STATUS "MPV: Found")
else()
    message(WARNING "MPV not found - media playback disabled")
endif()

# SQLite3 - link with proper target detection
if(SQLITE3_PKG_FOUND)
    target_link_libraries(podradio PRIVATE PkgConfig::SQLITE3_PKG)
    message(STATUS "SQLite3: Found via PkgConfig")
elseif(TARGET SQLite3::SQLite3)
    target_link_libraries(podradio PRIVATE SQLite3::SQLite3)
    message(STATUS "SQLite3: Found (SQLite3::SQLite3)")
elseif(TARGET SQLite::SQLite3)
    # Ubuntu uses this target name (without the '3')
    target_link_libraries(podradio PRIVATE SQLite::SQLite3)
    message(STATUS "SQLite3: Found (SQLite::SQLite3)")
elseif(SQLITE3_MANUAL)
    target_link_libraries(podradio PRIVATE ${SQLITE3_LIBRARY})
    message(STATUS "SQLite3: Found (${SQLITE3_LIBRARY})")
elseif(SQLite3_FOUND)
    # Fallback - try to get library from discovered variables
    if(DEFINED SQLite3_LIBRARIES)
        target_link_libraries(podradio PRIVATE ${SQLite3_LIBRARIES})
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
        target_link_libraries(podradio PRIVATE CURL::libcurl)
    elseif(CURL_LIBRARY)
        target_link_libraries(podradio PRIVATE ${CURL_LIBRARY})
    endif()
    message(STATUS "CURL: Found")
else()
    message(WARNING "CURL not found - network features disabled")
endif()

# libxml2
if(LibXml2_FOUND)
    if(TARGET LibXml2::LibXml2)
        target_link_libraries(podradio PRIVATE LibXml2::LibXml2)
    elseif(XML2_LIBRARY)
        target_link_libraries(podradio PRIVATE ${XML2_LIBRARY})
    endif()
    message(STATUS "libxml2: Found")
else()
    message(WARNING "libxml2 not found - RSS parsing disabled")
endif()

# fmt
if(fmt_FOUND)
    target_link_libraries(podradio PRIVATE fmt::fmt)
    message(STATUS "fmt: Found")
else()
    message(STATUS "fmt: Using bundled version")
endif()

# nlohmann_json
if(nlohmann_json_FOUND)
    target_link_libraries(podradio PRIVATE nlohmann_json::nlohmann_json)
    message(STATUS "nlohmann_json: Found")
else()
    message(STATUS "nlohmann_json: Using bundled version")
endif()

# ncurses
if(UNIX)
    if(CURSES_FOUND)
        target_link_libraries(podradio PRIVATE ${CURSES_LIBRARIES})
        target_include_directories(podradio PRIVATE ${CURSES_INCLUDE_DIR})
        message(STATUS "ncurses: Found")
    elseif(NCURSES_LIBRARY)
        target_link_libraries(podradio PRIVATE ${NCURSES_LIBRARY})
        message(STATUS "ncurses: Found (${NCURSES_LIBRARY})")
    else()
        message(WARNING "ncurses not found - TUI disabled")
    endif()
endif()

# =============================================================================
# 安装
# =============================================================================
install(TARGETS podradio
    RUNTIME DESTINATION bin
)

install(FILES README.md LICENSE
    DESTINATION share/doc/podradio
)

# 安装 man 手册页
if(UNIX)
    install(FILES man/podradio.1
        DESTINATION share/man/man1
    )
endif()

# =============================================================================
# 打包
# =============================================================================
set(CPACK_PACKAGE_NAME "PodRadio")
set(CPACK_PACKAGE_VERSION ${PODRADIO_FULL_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Terminal Podcast/Radio Player")
set(CPACK_PACKAGE_VENDOR "Panic")
set(CPACK_PACKAGE_CONTACT "Deadship2003@gmail.com")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "mpv,libsqlite3-0,libcurl4,libxml2,libncurses6")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)

# =============================================================================
# 配置摘要
# =============================================================================
message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "  PodRadio ${PODRADIO_FULL_VERSION} Configuration Summary")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "  Platform:     ${PODRADIO_PLATFORM}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "  Dependencies:")
message(STATUS "    MPV:       ${MPV_FOUND}")
message(STATUS "    SQLite3:   ${SQLite3_FOUND} (pkg=${SQLITE3_PKG_FOUND})"
message(STATUS "    CURL:      ${CURL_FOUND}")
message(STATUS "    libxml2:   ${LibXml2_FOUND}")
message(STATUS "    ncurses:   ${CURSES_FOUND}")
message(STATUS "    fmt:       ${fmt_FOUND}")
message(STATUS "    json:      ${nlohmann_json_FOUND}")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "")
