#!/bin/bash
# ==============================================================================
# Bukapilot K1S - Baseline Pure-Software Test Suite Runner
# ==============================================================================
set -u

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT="$(cd $DIR/../../ && pwd)"
cd "$ROOT"

export PYTHONPATH="$ROOT:${PYTHONPATH:-}"
export ZMQ=1
export NOSENSOR=1

echo "======================================================================"
echo "          Bukapilot K1S - Pure-Software Baseline Test Suite           "
echo "======================================================================"
echo "Workspace: $ROOT"
echo "Python:    $(python3 --version 2>&1)"
echo "Date:      $(date)"
echo "======================================================================"
echo ""

PASSED=0
FAILED=0
TOTAL=0

run_test() {
    local name="$1"
    local cmd="$2"
    TOTAL=$((TOTAL + 1))
    echo "----------------------------------------------------------------------"
    echo "[TEST $TOTAL] Running: $name"
    echo "Command: $cmd"
    echo "----------------------------------------------------------------------"
    
    if eval "$cmd"; then
        echo ""
        echo ">>> RESULT: [PASS] $name"
        PASSED=$((PASSED + 1))
    else
        echo ""
        echo ">>> RESULT: [FAIL] $name"
        FAILED=$((FAILED + 1))
    fi
    echo ""
}

# 1. Car Interfaces Test (Parameters, lateral/longitudinal tunings, update/apply loops)
run_test "Car Interfaces Verification" "python3 selfdrive/car/tests/test_car_interfaces.py"

# 2. Fingerprints Consistency Test (CAN message ID uniqueness and overlap checks)
run_test "Fingerprints Consistency" "python3 selfdrive/test/test_fingerprints.py"

# 3. Manager Test (Process config and supervisor logic)
run_test "Process Manager" "python3 selfdrive/manager/test/test_manager.py"

# 4. Simple Kalman Filter Test (State estimator mathematics)
run_test "Simple Kalman Filter" "python3 common/kalman/tests/test_simple_kalman.py"

# 5. Log Reader Utilities Test (Framereader and route log readers)
run_test "Log Reader Utilities" "python3 tools/lib/tests/test_readers.py"

echo "======================================================================"
echo "                         TEST SUMMARY REPORT                          "
echo "======================================================================"
echo "Total Tests Run:  $TOTAL"
echo "Passed:           $PASSED"
echo "Failed:           $FAILED"
echo "======================================================================"

if [ "$FAILED" -eq 0 ]; then
    echo "STATUS: ALL BASELINE TESTS PASSED."
    exit 0
else
    echo "STATUS: SOME TESTS FAILED. Review output above for details."
    exit 1
fi
