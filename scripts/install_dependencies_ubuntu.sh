#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
  echo "This script currently supports Ubuntu/Debian systems with apt-get." >&2
  exit 1
fi

SUDO=""
if [ "${EUID}" -ne 0 ]; then
  SUDO="sudo"
fi

$SUDO apt-get update
$SUDO apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  clang \
  llvm \
  llvm-dev \
  libclang-dev

echo "DummyCpp dependencies installed."

