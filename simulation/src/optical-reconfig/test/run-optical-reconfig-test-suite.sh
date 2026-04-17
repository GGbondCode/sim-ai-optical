#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_JOBS="${BUILD_JOBS:-8}"

cd "${SIM_ROOT}"

./ns3 configure --enable-examples --enable-tests
./ns3 build -j "${BUILD_JOBS}" test-runner
./ns3 run "test-runner --suite=optical-reconfig --verbose"
