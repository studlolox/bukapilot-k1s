# Bukapilot K1S - Development & Testing Environment

> **Branch:** `k1s-independent`  
> **Frozen Reference:** `k1s-baseline` (Commit `e470f343c`)  
> **Target Python:** Python `3.8.10`  
> **Target OS:** Ubuntu 20.04 LTS (Canonical Openpilot 0.8 Baseline)  

---

## 1. Overview & Architecture

Bukapilot K1S relies on a specific set of runtime dependencies and native extensions:
- **Cap'n Proto RPC / Cereal (`pycapnp == 1.1.0`)**: Compiled messaging schema interfaces for all inter-process communication.
- **Cython C-Extensions (`Cython == 0.29.36`)**: Compiled high-performance modules (`simple_kalman_impl.pyx`, `clock.pyx`, `params_pyx.pyx`, `messaging_pyx.pyx`).
- **ZeroMQ (`pyzmq == 22.3.0`)**: High-throughput message queuing.

Because modern host environments (e.g. macOS with Python 3.13+) lack the historical Python 3.8 toolchain and vendor binaries, development and test execution are containerized using an isolated Linux environment ([`Dockerfile.k1s-dev`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/Dockerfile.k1s-dev)).

---

## 2. Host Prerequisites

Install any container runtime on the developer host:
- **macOS / Windows**: [Docker Desktop](https://www.docker.com/products/docker-desktop/), [OrbStack](https://orbstack.dev/), or [Podman](https://podman.io/).
- **Linux**: Docker Engine (`docker.io` or `docker-ce`).

---

## 3. Quickstart Workflow

### Step 1: Build the Development Container Image
From the repository root:

```bash
docker build -t bukapilot-k1s-dev -f Dockerfile.k1s-dev .
```

### Step 2: Launch the Development Shell
Mount the repository into `/tmp/openpilot`:

```bash
docker run --rm -v "$(pwd)":/tmp/openpilot -it bukapilot-k1s-dev
```

### Step 3: Compile Extensions (Inside Container)
Before running tests, compile the Cereal Cap'n Proto schemas and Cython extensions:

```bash
bash tools/dev/build_ext.sh
```

### Step 4: Execute the Pure-Software Baseline Test Suite
Run all five pure-software baseline test suites in sequence:

```bash
bash tools/dev/run_tests.sh
```

---

## 4. One-Liner Test Commands (from Host)

To compile and run tests directly in a single command without entering the container interactively:

### Run Full Test Suite
```bash
docker run --rm -v "$(pwd)":/tmp/openpilot bukapilot-k1s-dev bash -c "bash tools/dev/build_ext.sh && bash tools/dev/run_tests.sh"
```

### Run Individual Test Suites
- **Car Interfaces Verification:**
  ```bash
  docker run --rm -v "$(pwd)":/tmp/openpilot bukapilot-k1s-dev bash -c "bash tools/dev/build_ext.sh && python3 selfdrive/car/tests/test_car_interfaces.py"
  ```
- **Fingerprint Consistency:**
  ```bash
  docker run --rm -v "$(pwd)":/tmp/openpilot bukapilot-k1s-dev bash -c "bash tools/dev/build_ext.sh && python3 selfdrive/test/test_fingerprints.py"
  ```
- **Process Manager Test:**
  ```bash
  docker run --rm -v "$(pwd)":/tmp/openpilot bukapilot-k1s-dev bash -c "bash tools/dev/build_ext.sh && python3 selfdrive/manager/test/test_manager.py"
  ```
- **Simple Kalman Filter Test:**
  ```bash
  docker run --rm -v "$(pwd)":/tmp/openpilot bukapilot-k1s-dev bash -c "bash tools/dev/build_ext.sh && python3 common/kalman/tests/test_simple_kalman.py"
  ```
- **Log Reader Utilities Test:**
  ```bash
  docker run --rm -v "$(pwd)":/tmp/openpilot bukapilot-k1s-dev bash -c "bash tools/dev/build_ext.sh && python3 tools/lib/tests/test_readers.py"
  ```

---

## 5. VS Code Dev Container Workflow

If using VS Code with the Remote Containers extension:
1. Open the project folder in VS Code.
2. Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on macOS) $\rightarrow$ **Remote-Containers: Reopen in Container**.
3. VS Code builds the image using [`.devcontainer/devcontainer.json`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/.devcontainer/devcontainer.json) and automatically triggers [`tools/dev/build_ext.sh`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/tools/dev/build_ext.sh) on container creation.

---

## 6. Test Suite Matrix & Safety Boundaries

| Test Script | Execution Environment | Safety Impact | Notes |
| :--- | :--- | :--- | :--- |
| [`selfdrive/car/tests/test_car_interfaces.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/car/tests/test_car_interfaces.py) | **Container / Pure-Software** | Safe (Read/Mock) | Validates `CarParams`, lateral PID/INDI tunings, and `CarInterface.update()` / `apply()` steps. |
| [`selfdrive/test/test_fingerprints.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/test/test_fingerprints.py) | **Container / Pure-Software** | Safe (Read-only) | Checks CAN ID consistency and verifies no conflicting fingerprints. |
| [`selfdrive/manager/test/test_manager.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/manager/test/test_manager.py) | **Container / Pure-Software** | Safe (Mocked) | Tests process lifecycle configs and daemon configurations. |
| [`common/kalman/tests/test_simple_kalman.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/common/kalman/tests/test_simple_kalman.py) | **Container / Pure-Software** | Safe (Math unit test) | Tests 1D Kalman filter state estimation math. |
| [`tools/lib/tests/test_readers.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/tools/lib/tests/test_readers.py) | **Container / Pure-Software** | Safe (Log parsing) | Validates log and framereader file parsers. |
| [`selfdrive/test/qc_test.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/test/qc_test.py) | **Hardware-Only (K1S EON)** | Requires vehicle ignition | Gated by live Panda ignition line and battery current sensors. |
| [`selfdrive/test/test_onroad.py`](file:///Users/solehuddin/Documents/Projects/bukapilot-k1s/selfdrive/test/test_onroad.py) | **Hardware-Only (K1S EON)** | Requires device daemons | Requires physical cameras, SNPE DSP runtimes, and shared memory IPC. |
