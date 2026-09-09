# AGENTS.md — Bukapilot K1S Independent Fork

## 1. Project Identity

This repository is an independent K1S fork based on the public KommuAI
Bukapilot `ka1s_snapshot` branch.

The project is intended to preserve K1S hardware compatibility while
gradually removing unnecessary dependency on Kommu infrastructure and
developing an independently maintainable software stack.

### Canonical Baseline

The original baseline is:

- Upstream repository: `https://github.com/kommuai/bukapilot.git`
- Upstream branch: `ka1s_snapshot`
- Local reference branch: `k1s-baseline`

### Active Development Branch

Normal development happens on:

- `k1s-independent`

`k1s-baseline` is a frozen reference and MUST NOT be modified.

---

### Target Hardware Platform: KommuAssist K1S

The primary hardware target for this repository is the **KommuAssist K1S**:
- **Platform Base**: LeEco Le Pro 3 (`LEX720` / `ZL1`) / comma EON Gold hardware architecture
- **System-on-Chip (SoC)**: Qualcomm Snapdragon 821 (`MSM8996 Pro`), 64-bit Quad-Core Kryo (2x 2.35 GHz + 2x 2.18 GHz)
- **GPU & Graphics**: Qualcomm Adreno 530 (`b00000.qcom,kgsl-3d0`)
- **Memory (RAM) & Storage**: 4GB / 6GB LPDDR4 RAM, 32GB / 64GB UFS 2.0 internal flash storage
- **Screen Size**: 5.5-inch IPS LCD capacitive touchscreen (~73.8% screen-to-body ratio)
- **Screen Resolution**: 1920 × 1080 pixels (Full HD 1080p, 16:9 aspect ratio, ~403 ppi)
- **Operating System / Environment**: Comma NEOS 19.1 (custom stripped Android 6.0.1 Marshmallow kernel 3.18 + Termux GNU/Linux userspace)
- **Integrated MCU**: Embedded STM32 Panda MCU inside device casing (auto-flashed with `panda/board/obj/icptr.bin.signed` interceptor firmware)
- **Camera Sensors**: Sony IMX298 (16 MP road-facing camera) + OmniVision OV8856 (driver-monitoring camera)
- **CPU & Power Governance**: Real-time CPU isolation locking Core 3 for CAN & controls via `rtshield.py`, with dynamic devfreq memory bus/GPU governors (`soc:qcom,cpubw`, `soc:qcom,m4m`, `b00000.qcom,kgsl-3d0`) for offroad thermal protection vs. onroad performance throughput

---

### Primary Vehicle Target: Toyota Corolla Cross (TSS2)

Active development and build focus in this repository is centered on the **Toyota Corolla Cross equipped with Toyota Safety Sense 2.0 (TSS2)**.

Key guidelines for Corolla Cross TSS2 focus:
- **Architecture & Bus Topology**:
  - TSS2 / No-DSU architecture using DBCs `toyota_nodsu_pt_generated` and `toyota_tss2_adas`.
  - Camera bus (bus 2 / ADAS) and Powertrain bus (bus 0 / PT) configuration via Panda.
- **Lateral Control & EPS**:
  - Parameters: Wheelbase 2.64 m, steer ratio 13.9, tire stiffness 0.444, lateral PID tuning (`LatTunes.PID_D`).
  - Optimize steering torque limits and delta rates (`STEER_DELTA_UP`, `STEER_DELTA_DOWN`, `STEER_ERROR_MAX`) without triggering EPS cutoffs.
  - Support stock LDA/LDP passthrough (`stockLdw` / `CS.out.stockAdas`).
- **Longitudinal Control**:
  - Support full stop-and-go (`stop_and_go = True`).
  - Longitudinal tuning: `LongTunes.CROSS_HYBRID` / `LongTunes.TSS2` with smooth decel rate (0.3 m/s²).
  - Robust handling of stock ACC passthrough (`StockAcc` / `CS.stock_acc_cmd`) vs openpilot longitudinal control.
- **Fingerprinting & Firmware Identification**:
  - Ensure robust identification of Corolla Cross EPS, Forward Camera, and Radar ECUs via FW queries, with clean fallback and `FixFingerprint` support for both Hybrid (`CAR.CROSSH_TSS2`) and Petrol variants.
- **Other Platforms**: Code for non-Toyota platforms (e.g., Perodua, Proton, Honda) is secondary; changes must not compromise or regress Corolla Cross TSS2 stability, safety, or functionality.

---

# 2. Core Agent Principles

The agent MUST prioritize:

1. Safety
2. Existing K1S functionality
3. Correctness
4. Reproducibility
5. Minimal changes
6. Maintainability
7. Performance
8. Developer convenience

Do not optimize for the smallest diff if doing so reduces correctness,
observability, or maintainability.

Do not make speculative architectural changes.

Do not assume that newer openpilot/Bukapilot code is automatically
compatible with this K1S baseline.

---

# 3. Safety-Critical Software Policy

This repository controls or interfaces with vehicle systems.

Treat the following as SAFETY-CRITICAL:

- Steering control
- Steering torque
- Steering angle
- LKA/LKS
- Lane change logic
- Assisted Lane Change (ALC)
- Lane Departure Warning/Prevention
- Longitudinal control
- ACC
- Acceleration commands
- Deceleration commands
- Brake-related commands
- Vehicle engagement/disengagement
- CAN messages
- CAN safety configuration
- Panda safety behaviour
- Actuator limits
- Torque limits
- Speed limits affecting control activation
- Vehicle fingerprints
- DBC definitions
- CarState
- CarController
- Safety hooks
- Control loop timing

## Rules for safety-critical code

The agent MUST NOT:

- Change safety limits casually.
- Increase actuator limits without explicit authorization.
- Disable safety checks.
- Bypass safety hooks.
- Remove vehicle-state validation.
- Modify CAN safety behaviour as a "cleanup".
- Change control-loop timing without understanding the consequences.
- Change steering/longitudinal behaviour merely to fix a test.
- Port safety-critical code from another version without compatibility
  analysis.

Before modifying safety-critical code, the agent MUST:

1. Identify the exact subsystem.
2. Explain current behaviour.
3. Explain the intended behaviour.
4. Identify all callers/dependencies.
5. Identify relevant tests.
6. Explain potential safety implications.
7. Make the smallest reasonable change.
8. Run all relevant tests that are available.

If the requested change is ambiguous, STOP and ask for clarification.

---

# 4. Baseline Protection

The branch `k1s-baseline` is a historical reference.

NEVER:

- Commit to `k1s-baseline`
- Rebase `k1s-baseline`
- Reset `k1s-baseline`
- Force push `k1s-baseline`
- Rewrite its history
- Delete the branch

The agent MUST work on:

`k1s-independent`

unless the user explicitly specifies another development branch.

---

# 5. Git Safety

The agent MUST NOT execute any of the following without explicit user
authorization:

```bash
git reset --hard
git clean -fd
git clean -fdx
git push --force
git push --force-with-lease
git rebase
git filter-repo
git filter-branch
git branch -D
git gc
git reflog expire