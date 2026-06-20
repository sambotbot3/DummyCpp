#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TEST_BUILD_DIR="${BUILD_DIR}/tests"
INJECT_LIB="${BUILD_DIR}/inject/libdpp_inject.a"

mkdir -p "${TEST_BUILD_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

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
  cc "${c_file}" -I "${ROOT_DIR}/inject/c" "${INJECT_LIB}" -o "${c_exe}"

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

unsupported_count=0
if compgen -G "${ROOT_DIR}/tests/unsupported/*.cpp" >/dev/null; then
  for cpp in "${ROOT_DIR}"/tests/unsupported/*.cpp; do
    unsupported_count=$((unsupported_count + 1))
    name="$(basename "${cpp}" .cpp)"
    case_dir="${TEST_BUILD_DIR}/unsupported_${name}"
    mkdir -p "${case_dir}"

    set +e
    "${BUILD_DIR}/dpp" "${cpp}" -o "${case_dir}/${name}.generated.c" >"${case_dir}/dpp.stdout" 2>"${case_dir}/dpp.stderr"
    status=$?
    set -e

    if [ "${status}" -eq 0 ]; then
      echo "FAIL unsupported/${name}: dpp unexpectedly succeeded" >&2
      exit 1
    fi

    if ! grep -Eq "syntax check failed|preprocess failed|template instantiation failed" "${case_dir}/dpp.stderr"; then
      echo "FAIL unsupported/${name}: missing expected diagnostic" >&2
      exit 1
    fi

    echo "PASS unsupported/${name}"
  done
fi

if [ "${unsupported_count}" -gt 0 ]; then
  echo "All ${unsupported_count} unsupported case(s) rejected."
fi
