#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXPECTED_RE2C_VERSION="4.5.1"

if ! command -v re2c >/dev/null 2>&1; then
  echo "re2c is required to generate lexer sources." >&2
  exit 1
fi

installed_version="$(re2c --version | awk '{print $2}')"
if [ "${installed_version}" != "${EXPECTED_RE2C_VERSION}" ]; then
  echo "warning: expected re2c ${EXPECTED_RE2C_VERSION}, found ${installed_version}" >&2
fi

shopt -s nullglob
sources=(
  "${ROOT_DIR}"/lib/parser/*.re
  "${ROOT_DIR}"/src/parser/*.re
)

if [ "${#sources[@]}" -eq 0 ]; then
  echo "No lexer sources found under lib/parser or src/parser." >&2
  exit 1
fi

for source in "${sources[@]}"; do
  output="${source%.re}.cpp"
  re2c --input custom -o "${output}" "${source}"
  echo "generated ${output#${ROOT_DIR}/}"
done
