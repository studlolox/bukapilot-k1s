#!/bin/bash
# ==============================================================================
# Bukapilot K1S - Build Development Extensions (Cython & Cereal Messaging)
# ==============================================================================
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT="$(cd $DIR/../../ && pwd)"
cd "$ROOT"

echo "=== Building Bukapilot K1S Baseline Extensions ==="

# Check if scons is available
if ! command -v scons &> /dev/null; then
    echo "ERROR: 'scons' build tool not found in PATH."
    echo "Please ensure you are running inside the K1S development container or an environment with scons installed."
    exit 1
fi

# Build cereal (Cap'n Proto schemas and messaging wrappers)
echo "--> Compiling cereal Cap'n Proto schemas and messaging..."
scons -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) cereal/

# Build common utilities and Cython extensions (clock, params_pyx, simple_kalman)
echo "--> Compiling common Cython extensions..."
scons -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) common/

echo "=== Build Completed Successfully ==="
