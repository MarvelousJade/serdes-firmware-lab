# Rework record

This is maintenance of an existing project, not an original-development history.
Branch: `shaoyu/rework`, starting at `37f837f`. Existing uncommitted README,
reference-document, editor, and driver line-ending changes are user-owned and
are excluded from rework commits. A fresh out-of-tree build avoids changing
existing build directories.

## Inspection and scope

Read the six library sources, shared headers, CLI, C++ tests, Python reference
and runner/tests, CMake, CI, documentation, and recent Git history. The existing
controller → driver → register backend boundary is appropriate. Keep the two
libraries, synchronous flow, fixed-size reports, seed replay, CLI/JSON, and
analytical Python checks. Earlier commits already removed unnecessary helpers
and clarified names; do not repeat that work or invent development history.

Baseline local Release verification: 104 C++ checks, 5 Python tests, 6 smoke
scenarios passed. The linker printed a `.sframe` diagnostic while returning
success; executables built and ran. No remote CI or Windows rerun is claimed.

Checklist / acceptance:
- [x] Inspect and establish a passing baseline.
- [x] Reject invalid controller policy before device I/O, retain normal bring-up.
- [x] Make health evidence current and test recovery/hysteresis.
- [ ] Require consistent subprocess status and JSON in regression evidence.
- [ ] Run full matrix/sanitizers; update concise architecture/interview evidence.
- [ ] Prepare isolated learning checkpoints and separately verified solutions.

## Increment 1: configuration boundary

Problem: no policy validation; NaN makes ordered comparisons false, including
`upper_bound > maximum_ber`, potentially bypassing acceptance. Zero lengths and
stability counts have unclear semantics.

Alternatives: throw in the constructor, silently clamp, or return an explicit
bring-up fault. Chosen: a short validation predicate and `InvalidConfiguration`
before reset/I/O. This matches the existing report-based failure contract and
avoids silently changing requested policy. Floating limits must be finite;
deadband is [0,1], BER limit is (0,1], measurement and training/stability/health
counts are positive. A zero PLL tick budget still allows an immediate readiness
check. A training budget smaller than stability count remains a valid deliberate
non-convergence scenario.

Evidence: the new 12-case regression produced 36 failed assertions before the
fix (wrong fault, extra trace states, and device writes). After the fix, Release
CTest passed all three groups: 164 C++ checks, 5 Python tests, 6 smoke scenarios.
No algorithm or register layout changed. Caller budgets are not a wall-clock
service guarantee; very large windows remain impractical.

## Increment 2: health evidence lifecycle

Problem: a valid measurement from the previous link/session survived bring-up,
lost PLL, and skipped checks. Consumers could display stale BER as current.
Alternatives: attach generation IDs/timestamps, retain historical telemetry with
an explicit age, or invalidate at each operation boundary. Chosen: clear the
single saved measurement at bring-up and health-check entry. This keeps the API
small and gives `valid` an unambiguous latest-check meaning; historical readings
must be copied by the caller if needed.

Evidence: new lifecycle/recovery tests failed four assertions before the fix.
After two reset assignments, 187 C++ checks, 5 Python tests, and 6 smoke scenarios
passed. Tests also exercise bad/bad/healthy/bad/bad/bad hysteresis, a skipped check
in Degraded, explicit retraining, lost PLL, and a zero-length health fault. No
automatic retrain scheduler was added: the caller remains responsible for recovery.
