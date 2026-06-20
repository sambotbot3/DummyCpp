#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
EXAMPLE_DIR="${ROOT_DIR}/examples/multifile_supported"
CASE_DIR="${BUILD_DIR}/examples/multifile_supported"
INJECT_LIB="${BUILD_DIR}/inject/libdpp_inject.a"

sources=(main scores counts ordering)

mkdir -p "${CASE_DIR}/generated"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

cpp_inputs=()
c_inputs=()
for source in "${sources[@]}"; do
  cpp_inputs+=("${EXAMPLE_DIR}/${source}.cpp")
  c_file="${CASE_DIR}/generated/${source}.c"
  "${BUILD_DIR}/dpp" "${EXAMPLE_DIR}/${source}.cpp" -o "${c_file}"
  c_inputs+=("${c_file}")
done

c++ -std=c++11 "${cpp_inputs[@]}" -o "${CASE_DIR}/multifile.cpp.exe"
cc "${c_inputs[@]}" -I "${ROOT_DIR}/inject/c" "${INJECT_LIB}" -o "${CASE_DIR}/multifile.c.exe"

run_capture() {
  local exe="$1"
  local stdout_file="$2"
  local status_file="$3"

  set +e
  "${exe}" >"${stdout_file}"
  local status=$?
  set -e

  printf '%s\n' "${status}" >"${status_file}"
}

run_capture "${CASE_DIR}/multifile.cpp.exe" "${CASE_DIR}/cpp.stdout" "${CASE_DIR}/cpp.status"
run_capture "${CASE_DIR}/multifile.c.exe" "${CASE_DIR}/c.stdout" "${CASE_DIR}/c.status"

diff -u "${CASE_DIR}/cpp.stdout" "${CASE_DIR}/c.stdout"
diff -u "${CASE_DIR}/cpp.status" "${CASE_DIR}/c.status"

echo "PASS multifile_supported example"
