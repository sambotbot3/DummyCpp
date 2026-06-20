#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
INJECT_LIB="${BUILD_DIR}/inject/libdpp_inject.a"
EXAMPLE_DIR="${ROOT_DIR}/examples/multifile_known_fail"
CASE_DIR="${BUILD_DIR}/examples/multifile_known_fail"

sources=(main inventory reporting)

mkdir -p "${CASE_DIR}/generated"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

cpp_inputs=()
for source in "${sources[@]}"; do
  cpp_inputs+=("${EXAMPLE_DIR}/${source}.cpp")
done

c++ -std=c++11 -I "${EXAMPLE_DIR}" "${cpp_inputs[@]}" -o "${CASE_DIR}/known_fail.cpp.exe"
"${CASE_DIR}/known_fail.cpp.exe" >"${CASE_DIR}/cpp.stdout"
cpp_status=$?
printf '%s\n' "${cpp_status}" >"${CASE_DIR}/cpp.status"

echo "C++ reference output:"
cat "${CASE_DIR}/cpp.stdout"

failure_count=0
for source in "${sources[@]}"; do
  c_file="${CASE_DIR}/generated/${source}.c"
  log_file="${CASE_DIR}/${source}.dpp.log"

  set +e
  "${BUILD_DIR}/dpp" "${EXAMPLE_DIR}/${source}.cpp" -o "${c_file}" >"${log_file}" 2>&1
  dpp_status=$?
  set -e

  if [[ "${dpp_status}" -eq 0 ]]; then
    echo "DPP PROGRESS: ${source}.cpp transpiled, but the full example is still known-fail"
  else
    failure_count=$((failure_count + 1))
    echo "EXPECTED DPP FAILURE: ${source}.cpp"
    sed -n '1,12p' "${log_file}"
  fi
done

if [[ "${failure_count}" -gt 0 ]]; then
  echo "PASS multifile_known_fail example documents current unsupported coverage"
  exit 0
fi

c_inputs=()
for source in "${sources[@]}"; do
  c_inputs+=("${CASE_DIR}/generated/${source}.c")
done

cc -std=c11 -I "${ROOT_DIR}/inject/c" "${c_inputs[@]}" "${INJECT_LIB}" -o "${CASE_DIR}/known_fail.c.exe"
"${CASE_DIR}/known_fail.c.exe" >"${CASE_DIR}/c.stdout"
c_status=$?
printf '%s\n' "${c_status}" >"${CASE_DIR}/c.status"

diff -u "${CASE_DIR}/cpp.stdout" "${CASE_DIR}/c.stdout"
diff -u "${CASE_DIR}/cpp.status" "${CASE_DIR}/c.status"

echo "PASS multifile_known_fail example matches generated C"
