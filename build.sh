#!/bin/bash
# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                    PodRadio 多平台编译脚本                                ║
# ╚═══════════════════════════════════════════════════════════════════════════╝
# 用法: ./build.sh [all|linux|arm64|windows|clean]

set -e
cd "$(dirname "$0")"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           PodRadio Multi-Platform Build Script                ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════════╝${NC}"

build_linux() {
    echo -e "\n${BLUE}[Linux x86_64] 编译中...${NC}"
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel $(nproc)
    echo -e "${GREEN}✓ 完成: build/podradio${NC}"
}

build_arm64() {
    echo -e "\n${BLUE}[Linux ARM64] 编译中...${NC}"
    if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠ 跳过: 需安装 gcc-aarch64-linux-gnu${NC}"
        return
    fi
    cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux.cmake
    cmake --build build-arm64 --parallel $(nproc)
    echo -e "${GREEN}✓ 完成: build-arm64/podradio${NC}"
}

build_windows() {
    echo -e "\n${BLUE}[Windows x64] 编译中...${NC}"
    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠ 跳过: 需安装 mingw-w64${NC}"
        return
    fi
    cmake -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-mingw.cmake
    cmake --build build-windows --parallel $(nproc)
    echo -e "${GREEN}✓ 完成: build-windows/podradio.exe${NC}"
}

case "${1:-all}" in
    all)    build_linux; build_arm64; build_windows ;;
    linux)  build_linux ;;
    arm64)  build_arm64 ;;
    windows) build_windows ;;
    clean)  rm -rf build build-arm64 build-windows ;;
    *)      echo "用法: $0 [all|linux|arm64|windows|clean]" ;;
esac
