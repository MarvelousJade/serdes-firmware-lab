# Actual local practice observations (not production incidents)

The fictional reports/consequences are in `docs/debugging-exercises.md`.
These are observed fixture outputs, not an explanation of the causes.

Before injecting any defect, the learning-only fixtures and existing tests
passed all 5 CTest groups against the working implementation.

## Checkpoint A

Release `serdes_practice health`, exit 1:

```text
window 1: expected Observe, observed Observe
window 2: expected Observe, observed Observe
window 3: expected Healthy, observed Healthy
window 4: expected Observe, observed RetrainRequired
window 5: expected Observe, observed NotLinkUp
window 6: expected RetrainRequired, observed NotLinkUp
```

CTest: 3/5 pass; `serdes_cpp_tests` fails 3/187 checks and `practice_health`
fails. `practice_replay`, 12 Python tests, and 6 real smoke cases still pass.
A normal startup smoke test alone is not enough to detect this behavior.

## Checkpoint B (includes both exercises)

Release `serdes_practice replay`, exit 1:

```text
first: valid=1, symbols=1025, errors=0, mse=0.0733642578125, margin=0.988052368164
repeat: valid=1, symbols=1025, errors=0, mse=0.0772094726562, margin=0.982009887695
identical measurements: no
```

CTest: 2/5 pass. Existing C++ suite fails 27/187 checks (24 repeated-measurement
comparisons plus the 3 action failures); both practice fixtures fail. The 12
Python tests and 6 startup smoke scenarios still pass. Both windows have zero
observed errors: checking only the error counter would miss the discrepancy.
