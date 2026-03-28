#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
CONFIGURE_PRESET="linux-release"
BUILD_PRESET="linux-release"
BUILD_DIR="${ROOT}/build-linux-release"
RUN_TESTS=1
FRESH=0

for arg in "$@"; do
  case "${arg}" in
    debug)
      BUILD_TYPE="Debug"
      CONFIGURE_PRESET="linux-debug"
      BUILD_PRESET="linux-debug"
      BUILD_DIR="${ROOT}/build-linux-debug"
      ;;
    release)
      BUILD_TYPE="Release"
      CONFIGURE_PRESET="linux-release"
      BUILD_PRESET="linux-release"
      BUILD_DIR="${ROOT}/build-linux-release"
      ;;
    clean)
      FRESH=1
      ;;
    notest)
      RUN_TESTS=0
      ;;
    *)
      echo "[ERROR] Unknown argument: ${arg}" >&2
      exit 1
      ;;
  esac
done

if [[ ${FRESH} -eq 1 ]]; then
  echo "[INFO] Removing previous build directory: ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi

if [[ -z "${QT_DIR:-}" && -z "${CMAKE_PREFIX_PATH:-}" ]]; then
  echo "[WARN] QT_DIR/CMAKE_PREFIX_PATH is not set. CMake must still be able to find your Qt 6 kit."
fi

echo "[INFO] Root:       ${ROOT}"
echo "[INFO] Build type: ${BUILD_TYPE}"
echo "[INFO] Build dir:  ${BUILD_DIR}"
echo "[INFO] Preset:     ${CONFIGURE_PRESET}"

cmake --preset "${CONFIGURE_PRESET}"
cmake --build --preset "${BUILD_PRESET}" --parallel

if [[ ${RUN_TESTS} -eq 1 ]]; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

echo
echo "[OK] Build complete."
echo "[OK] Executable: ${ROOT}/bin/jarvis"
echo "[OK] Logs:       ${ROOT}/bin/logs"
