#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

"${BUILD_DIR}/dpp" "${ROOT_DIR}/examples/point.cpp" -o "${BUILD_DIR}/point.c"
cc "${BUILD_DIR}/point.c" -o "${BUILD_DIR}/point"
set +e
"${BUILD_DIR}/point"
status=$?
set -e

echo "point example exit status: ${status}"
test "${status}" -eq 3
