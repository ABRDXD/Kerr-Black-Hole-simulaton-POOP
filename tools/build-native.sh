#!/usr/bin/env bash
# Build the independent C++ reference project; this is not the desktop app launcher.
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: bash tools/build-native.sh [options]

Default: Release, CPU tools and tests only; configure, build, then run CTest.

  --configuration Debug|Release|Sanitize
  --vulkan              Also build/test headless Vulkan geometry conformance
  --window              Also build/test the GLFW/Vulkan diagnostic window
  --require-validation  Require Khronos validation (needs --vulkan or --window)
  --jobs N              Maximum parallel build jobs (positive integer)
  --skip-tests          Build without running CTest; this does not validate the build
  -h, --help            Show this help

Requirements: Linux, CMake >= 3.25, Ninja, GCC with C++20 support, and CTest.
Vulkan: native glslang/glslangValidator, Vulkan headers, loader and compatible driver.
Window: bundled GLFW source, C compiler, Wayland/X11 development dependencies,
an active graphical desktop, and the required Vulkan presentation extensions.
Strict validation additionally needs the system Khronos validation layer.

The window target is a gradient/checkerboard diagnostic, not the black-hole app.
Use the separate desktop launcher for the Electron black-hole renderer.
USAGE
}

fail() { printf 'Error: %s\n' "$*" >&2; exit 2; }

configuration=Release
vulkan=OFF
window=OFF
require_validation=OFF
run_tests=true
jobs=()

while (($#)); do
    case "$1" in
        --configuration)
            (($# >= 2)) || fail '--configuration needs Debug, Release, or Sanitize'
            configuration=$2
            shift 2
            ;;
        --vulkan) vulkan=ON; shift ;;
        --window) window=ON; vulkan=ON; shift ;;
        --require-validation) require_validation=ON; shift ;;
        --jobs)
            (($# >= 2)) || fail '--jobs needs a positive integer'
            [[ $2 =~ ^[1-9][0-9]*$ ]] || fail '--jobs needs a positive integer'
            jobs=(--parallel "$2")
            shift 2
            ;;
        --skip-tests) run_tests=false; shift ;;
        -h|--help) usage; exit 0 ;;
        *) fail "Unknown option: $1 (use --help)" ;;
    esac
done

case "$configuration" in
    Debug|debug) preset=linux-gcc-debug ;;
    Release|release) preset=linux-gcc-release ;;
    Sanitize|sanitize) preset=linux-gcc-sanitize ;;
    *) fail '--configuration must be Debug, Release, or Sanitize' ;;
esac
if [[ $require_validation == ON && $vulkan != ON ]]; then
    fail '--require-validation needs --vulkan or --window'
fi
[[ $(uname -s) == Linux ]] || fail 'Run this native helper on Linux; use build.ps1 on Windows'

for command in cmake ctest ninja g++; do
    command -v "$command" >/dev/null 2>&1 || fail "Missing $command; install the Linux build dependencies first"
done
if [[ $window == ON ]]; then
    command -v gcc >/dev/null 2>&1 || fail 'The GLFW window build also needs GCC (C compiler)'
fi

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd -- "$project_root"
cmake_options=(
    "-DKERR_BUILD_VULKAN=$vulkan"
    "-DKERR_BUILD_RENDERER=$window"
    "-DKERR_REQUIRE_GPU_VALIDATION=$require_validation"
    -DKERR_BUILD_TESTS=ON
    -DKERR_BUILD_TOOLS=ON
)
if [[ $window == ON ]]; then
    cmake_options+=(-DCMAKE_C_COMPILER=gcc)
fi
if [[ $vulkan == ON ]]; then
    if ! command -v glslang >/dev/null 2>&1 && ! command -v glslangValidator >/dev/null 2>&1; then
        fail 'Vulkan builds need a native glslang or glslangValidator on PATH; Windows shader compiler binaries cannot run on Linux'
    fi
    bundled_headers=$project_root/.tools/Vulkan-Headers-2cd90f9d20df57eac214c148f3aed885372ddcfe/include
    if [[ -f $bundled_headers/vulkan/vulkan_core.h ]]; then
        cmake_options+=("-DKERR_VULKAN_INCLUDE_DIR=$bundled_headers")
    fi
    export VK_LAYER_ENABLES="${VK_LAYER_ENABLES:-VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT}"
fi

printf 'Configuring %s (Vulkan=%s, window=%s, required validation=%s)\n' \
    "$preset" "$vulkan" "$window" "$require_validation"
cmake --preset "$preset" "${cmake_options[@]}"
cmake --build --preset "$preset" "${jobs[@]}"
if [[ $run_tests == true ]]; then
    ctest --preset "$preset" --output-on-failure
else
    printf 'CTest skipped by request. Build success does not establish runtime or numerical validation.\n'
fi
