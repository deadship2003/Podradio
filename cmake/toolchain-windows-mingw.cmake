# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║            CMake Toolchain - Windows x64 (MinGW-w64) STATIC              ║
# ║                                                                           ║
# ║  Produces a fully static .exe (no DLL dependencies)                       ║
# ║                                                                           ║
# ║  Prerequisites (Debian/Ubuntu):                                            ║
# ║    sudo apt install mingw-w64-common gcc-mingw-w64-x86-64 \              ║
# ║                     g++-mingw-w64-x86-64                                  ║
# ║                                                                           ║
# ║  Optional: Cross-compiled dependencies via vcpkg or MXE:                  ║
# ║    - libcurl, libxml2, sqlite3, fmt, nlohmann_json, PDCurses              ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(WIN32 TRUE)
set(MINGW TRUE)

# ── Static linking: produce standalone .exe with no DLL dependencies ──
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

# Optimization flags for Release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# POSIX thread support (MinGW-w64 provides pthreads)
set(THREADS_PTHREADS_WIN32 TRUE)
