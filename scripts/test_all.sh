#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TEST_BUILD_DIR="${BUILD_DIR}/tests"

mkdir -p "${TEST_BUILD_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

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

case_count=0
for cpp in "${ROOT_DIR}"/tests/cases/*.cpp; do
  case_count=$((case_count + 1))
  name="$(basename "${cpp}" .cpp)"
  case_dir="${TEST_BUILD_DIR}/${name}"
  mkdir -p "${case_dir}"

  cpp_exe="${case_dir}/${name}.cpp.exe"
  c_file="${case_dir}/${name}.generated.c"
  c_exe="${case_dir}/${name}.c.exe"

  c++ -std=c++11 "${cpp}" -o "${cpp_exe}"
  "${BUILD_DIR}/dpp" "${cpp}" -o "${c_file}"
  cc "${c_file}" -o "${c_exe}"

  run_capture "${cpp_exe}" "${case_dir}/cpp.stdout" "${case_dir}/cpp.status"
  run_capture "${c_exe}" "${case_dir}/c.stdout" "${case_dir}/c.status"

  if ! diff -u "${case_dir}/cpp.stdout" "${case_dir}/c.stdout"; then
    echo "FAIL ${name}: stdout mismatch" >&2
    exit 1
  fi

  if ! diff -u "${case_dir}/cpp.status" "${case_dir}/c.status"; then
    echo "FAIL ${name}: exit status mismatch" >&2
    exit 1
  fi

  echo "PASS ${name}"
done

if [ "${case_count}" -eq 0 ]; then
  echo "No test cases found." >&2
  exit 1
fi

echo "All ${case_count} test case(s) passed."

