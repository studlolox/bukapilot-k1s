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