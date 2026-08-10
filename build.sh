#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

usage() {
  echo "Usage: $0 <debug|release|clean|all|flash [debug|release]>"
}

clean_build_dirs() {
  rm -rf "$ROOT_DIR/build"
  echo "Removed build directory: $ROOT_DIR/build"
}

run_build() {
  local build_type="$1"
  local build_dir="$2"

  ensure_toolchain_in_path

  local generator_args=()
  if command -v ninja >/dev/null 2>&1; then
    generator_args=(-G Ninja)
  fi

  rm -rf "$build_dir"
  mkdir -p "$build_dir"

  cmake -S "$ROOT_DIR" -B "$build_dir" \
    "${generator_args[@]}" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/arm-none-eabi-gcc.cmake" \
    -DCMAKE_BUILD_TYPE="$build_type"

  cmake --build "$build_dir" --parallel

  echo "Build finished: $build_type"
  echo "Artifacts directory: $build_dir"
}

ensure_toolchain_in_path() {
  if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    return 0
  fi

  local candidate
  for candidate in \
    "$HOME/.local/share/stm32cube/bundles/gnu-tools-for-stm32"/*/bin \
    /opt/st/stm32cubeide_*/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin; do
    if [[ -x "$candidate/arm-none-eabi-gcc" ]]; then
      export PATH="$candidate:$PATH"
      return 0
    fi
  done

  echo "error: arm-none-eabi-gcc not found in PATH" >&2
  echo "hint: install GNU Tools for STM32 or add the toolchain bin directory to PATH" >&2
  return 1
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

BUILD_MODE="${1,,}"
BUILD_TYPE=""
BUILD_DIR=""

case "$BUILD_MODE" in
  debug)
    BUILD_TYPE="Debug"
    BUILD_DIR="$ROOT_DIR/build/Debug"
    ;;
  release)
    BUILD_TYPE="Release"
    BUILD_DIR="$ROOT_DIR/build/Release"
    ;;
  clean)
    clean_build_dirs
    exit 0
    ;;
  all)
    run_build "Debug" "$ROOT_DIR/build/Debug"
    run_build "Release" "$ROOT_DIR/build/Release"
    exit 0
    ;;
  flash)
    FLASH_DIR="$ROOT_DIR/build/Debug"
    if [[ $# -eq 2 ]]; then
      case "${2,,}" in
        debug)   FLASH_DIR="$ROOT_DIR/build/Debug" ;;
        release) FLASH_DIR="$ROOT_DIR/build/Release" ;;
        *) echo "error: unknown variant '${2}', use debug or release" >&2; exit 1 ;;
      esac
    fi
    HEX_FILE="$(ls "$FLASH_DIR"/*.hex 2>/dev/null | head -1)"
    if [[ -z "$HEX_FILE" ]]; then
      echo "error: no .hex file found in $FLASH_DIR, run build first" >&2
      exit 1
    fi
    # 优先使用 STM32CubeProgrammer（对 STM32H5 支持更好）
    CUBE_PROG="$(find /opt/st/stm32cubeide_*/plugins -name "STM32_Programmer_CLI" -type f 2>/dev/null | head -1)"
    if [[ -n "$CUBE_PROG" ]]; then
      echo "Flashing (STM32CubeProgrammer): $HEX_FILE"
      "$CUBE_PROG" -c port=SWD reset=CoreRst -w "$HEX_FILE" -v -rst
    elif command -v st-flash >/dev/null 2>&1; then
      echo "Flashing (st-flash): $HEX_FILE"
      st-flash --format ihex write "$HEX_FILE"
    else
      echo "error: no flash tool found (STM32_Programmer_CLI or st-flash required)" >&2
      exit 1
    fi
    exit 0
    ;;
  *)
    usage
    exit 1
    ;;
esac

run_build "$BUILD_TYPE" "$BUILD_DIR"
