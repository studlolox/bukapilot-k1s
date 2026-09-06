# K1S Baseline Verification Report

> **Date:** 2026-08-26  
> **Repository:** Bukapilot K1S  
> **Development Branch:** `k1s-independent`  
> **Frozen Baseline Branch:** `k1s-baseline`  
> **Baseline Commit:** `e470f343c185a99e6915cbaad3e501f0c349276d`  
> **HEAD Commit:** `e470f343c185a99e6915cbaad3e501f0c349276d`  

---

## 1. Repository State & Integrity

### Branch & Commit Verification
- **Active Development Branch:** `k1s-independent`
- **Local Reference Baseline Branch:** `k1s-baseline` (Confirmed exists locally)
- **Baseline Commit Resolution:** `e470f343c185a99e6915cbaad3e501f0c349276d`
- **Current HEAD Commit:** `e470f343c185a99e6915cbaad3e501f0c349276d`
- **Branch Divergence Check:** `git diff k1s-baseline..k1s-independent` produces **0 lines of diff** (identical commit state).
- **Working Tree Status:** Clean with respect to tracked repository files.

---

## 2. Environment Information & Toolchain

### Host System
- **Operating System:** macOS 26.5.2 (Darwin 25.5.0 arm64)
- **Host Architecture:** `arm64` (Apple Silicon)
- **Host C/C++ Compiler:** Apple Clang 21.0.0 (`clang-2100.1.1.101`)
- **Host Python Executable:** `/usr/local/bin/python3` (Python 3.13.7)

### Repository Requirements vs Host Environment
| Component | Repository Target | Host System State | Status |
| :--- | :--- | :--- | :--- |
| **Python Version** | Python `3.8.10` ([`.python-version`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/.python-version), [`Pipfile`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/Pipfile)) | Python `3.13.7` (Global) | **ENVIRONMENT_MISMATCH** |
| **Package Manager** | `pipenv` / `pyenv` ([`update_requirements.sh`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/update_requirements.sh)) | Missing `pyenv`, `pipenv` | **MISSING_DEPENDENCY** |
| **Build System** | SCons (`SConstruct` targeting aarch64/EON, larch64/TICI, Linux x86_64) | SCons not installed | **MISSING_DEPENDENCY** |
| **Messaging Lib** | `pycapnp == 1.1.0` + Cap'n Proto C++ | Not installed in host Python 3.13 | **MISSING_DEPENDENCY** |
| **Realtime Extensions** | Cython (`.pyx`) compiled modules | Uncompiled on host | **BUILD_REQUIRED** |

---

## 3. Build Verification

### Native Build Assessment
- **Build Target Platform in Repository:**
  1. `aarch64`: Android 6 / Qualcomm MSM8996 (EON / K1S hardware running NEOS 19.1)
  2. `larch64`: Linux aarch64 / Qualcomm QCS615 / RB5 (TICI / Comma 3 running AGNOS)
  3. `x86_64`: Ubuntu 20.04 Linux (PC simulation & unit test container)
- **Darwin (macOS) Native Build Status:**
  - `SConstruct` has partial Darwin routing, but external prebuilt vendor binaries ([`third_party/acados/`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/third_party/acados), [`third_party/libyuv/`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/third_party/libyuv), [`third_party/snpe/`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/third_party/snpe)) are only provided for `aarch64`, `larch64`, and `x86_64-linux`.
  - Host currently lacks SCons build tool and required Cython build steps for Darwin.
- **Build Status Classification:** `ENVIRONMENT_FAILURE` (Host is macOS arm64 without the target cross-compilation toolchain or Linux x86_64 container).

---

## 4. Test Matrix & Baseline Execution Results

### Safe Non-Hardware Unit Tests

| Test Suite / Path | Execution Command | Result | Failure Reason | Failure Classification |
| :--- | :--- | :--- | :--- | :--- |
| **Car Interfaces**<br>[`selfdrive/car/tests/test_car_interfaces.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/car/tests/test_car_interfaces.py) | `PYTHONPATH=. python3 selfdrive/car/tests/test_car_interfaces.py` | **FAILED** | `ModuleNotFoundError: No module named 'parameterized'` | `MISSING_DEPENDENCY` |
| **Fingerprints**<br>[`selfdrive/test/test_fingerprints.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/test/test_fingerprints.py) | `PYTHONPATH=. python3 selfdrive/test/test_fingerprints.py` | **FAILED** | `ModuleNotFoundError: No module named 'serial'` (via `hardware.py`) | `MISSING_DEPENDENCY` |
| **Process Manager**<br>[`selfdrive/manager/test/test_manager.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/manager/test/test_manager.py) | `PYTHONPATH=. python3 selfdrive/manager/test/test_manager.py` | **FAILED** | `ModuleNotFoundError: No module named 'capnp'` (cereal messaging) | `MISSING_DEPENDENCY` |
| **Simple Kalman Filter**<br>[`common/kalman/tests/test_simple_kalman.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/common/kalman/tests/test_simple_kalman.py) | `PYTHONPATH=. python3 common/kalman/tests/test_simple_kalman.py` | **FAILED** | `ModuleNotFoundError: No module named 'common.kalman.simple_kalman_impl'` | `ENVIRONMENT_FAILURE` (Uncompiled Cython extension) |
| **Log Frame Readers**<br>[`tools/lib/tests/test_readers.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/tools/lib/tests/test_readers.py) | `PYTHONPATH=. python3 tools/lib/tests/test_readers.py` | **FAILED** | `ModuleNotFoundError: No module named 'lru'` | `MISSING_DEPENDENCY` |

---

### Hardware-Dependent / System Integration Tests (Safety Evaluation)

| Test Suite / Path | Feasibility Assessment | Safety Impact | Decision | Classification |
| :--- | :--- | :--- | :--- | :--- |
| **Quality Control Test**<br>[`selfdrive/test/qc_test.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/test/qc_test.py) | Requires physical Panda ignition CAN signal, thermal sensors, and battery telemetry | Modifies `Params("QC_Test")`, blocks on live CAN ignition | **SKIPPED** (Cannot run without hardware) | `EXPECTED_HARDWARE_FAILURE` |
| **Onroad Integration Test**<br>[`selfdrive/test/test_onroad.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/test/test_onroad.py) | Spawns full `manager.py` process suite (`camerad`, `modeld`, `boardd`, `loggerd`, `controlsd`) | Requires compiled native daemons, shared memory IPC, camera/CAN sensors | **SKIPPED** (Cannot run without build and Linux runtime) | `EXPECTED_HARDWARE_FAILURE` |

---

## 5. Summary of Test Statistics

- **Total Targeted Tests:** 7
- **Tests Executed:** 5
- **Tests Passed:** 0
- **Tests Failed:** 5 (Due to missing Python 3.8 virtualenv / uncompiled Cython extensions on host)
- **Tests Skipped (Hardware-Gated):** 2 (`qc_test.py`, `test_onroad.py`)

### Failure Classification Breakdown
- `MISSING_DEPENDENCY`: 4 (`parameterized`, `pyserial`, `pycapnp`, `lru-dict`)
- `ENVIRONMENT_FAILURE`: 1 (`common.kalman.simple_kalman_impl` Cython C extension build required)
- `EXPECTED_HARDWARE_FAILURE`: 2 (`qc_test.py`, `test_onroad.py`)
- `BASELINE_FAILURE`: 0 (No baseline code defect identified)

---

## 6. Known Hardware-Only & Platform Limitations

1. **Hardware MCU Flashing:**
   - Panda flashing (`pandad.py`) interacts directly with USB DFU and STM32 hardware on K1S devices. Must remain untouched during non-hardware tasks.
2. **Real-time CPU Isolation (`rtshield`):**
   - Requires Qualcomm Snapdragon Linux/Android sysfs paths (`/dev/cpuset`, `/sys/devices/system/cpu/`) and root permissions.
3. **Vision Pipeline (`camerad` / `modeld`):**
   - Depends on Qualcomm Snapdragon Spectra ISP and SNPE (Snapdragon Neural Processing Engine) DSP runtime binaries (`libsnpe_dsp.so`), which do not execute natively on Darwin/Apple Silicon.

---

## 7. Recommended Next Steps

To proceed with reliable, isolated development without risking regressions:

1. **Step 1: Hermetic Development & Test Environment**
   - Establish a standard Python 3.8 runtime matching `.python-version` and install developer dependencies from `Pipfile` (e.g. `pycapnp`, `pyserial`, `parameterized`, `pyzmq`, `cython`, `pytest`).
   - Compile local Cython modules (`common/kalman/simple_kalman_impl.pyx`, `common/clock.pyx`, `common/params_pyx.pyx`).
2. **Step 2: Re-run & Pass Pure Software Baseline Tests**
   - Verify that `test_fingerprints.py` and `test_car_interfaces.py` pass 100% cleanly in the isolated environment.
3. **Step 3: Begin Phase 2 Isolation (Authentication Decoupling)**
   - Once baseline testing passes locally, proceed to decouple Ory Kratos cloud authentication in [`selfdrive/athena/registration.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/athena/registration.py).
