# Instructor material — spoilers

Kept only on `shaoyu/learning-solutions`. Do not read before investigating.
All hypotheses below are example reasoning for practice, not learner experience.

## A: progressive hints

1. Compare the meaning of "consecutive" with the six action outputs. When does
   a new run of failing windows begin?
2. Inspect persistent controller state before and after the healthy window.
3. The healthy path must clear the bad-window counter, not just return Healthy.

Diagnosis: removing that reset turns a consecutive-failure count into accumulated
failures across healthy intervals. The next bad window triggers Degraded early;
subsequent checks then return NotLinkUp. Bring-up alone still passes.

Fix: reset `consecutive_bad_windows_` on the healthy path. The health fixture is
a failing behavioral regression at checkpoint A and passes after the fix.
Existing `test_degradation_hysteresis` independently covers the six-window
sequence, skipped checks and explicit recovery. The policy is still three
consecutive bad windows, not a lifetime count or moving average.

Actual local Release verification after the A fix while B remains faulty:
health fixture passes all six expected actions, exit 0; CTest 3/5 groups pass.
C++ failures fall from 27 to 24, all remaining replay comparisons. Python's 12
tests and 6 real smoke cases pass. The remaining failures are accurately recorded,
not declared a passing combined solution.

## B: progressive hints

1. A repeatable sequence includes more than the PRBS bits. List the hidden state
   read by one symbol step, and compare even- and odd-sized windows.
2. The Gaussian generator produces two samples per transform. What survives
   between calls, and what does restarting the uniform generator alone change?
3. Invalidate the cached Gaussian sample on every sequence restart.

Diagnosis: Box–Muller retains a second sample with a validity flag. An odd-sized
window leaves a valid cached sample. Resetting the noise PRNG but not its cache
consumes a sample from the previous sequence before resuming the new sequence,
shifting noise/history relative to the data. Bit errors may remain zero while
MSE, margin and correlations change.

Fix: clear the cached sample and its validity flag in `restart_test_sequence()`.
The crucial part is invalidation; zeroing the value alone leaves a usable but
incorrect zero sample. Keep equalizer settings and PLL readiness intact: restart
is not a full device reset. The 1,025-symbol fixture compares every decoded
measurement field, and the existing replay regression repeats across three
profiles, both modes and four seeds, including zero-seed fallback.

Actual final verification after both prepared fixes:
- Release CTest: 5/5 groups pass, 187 C++ checks, both fixtures, 12 Python tests,
  and 6 real CLI smoke scenarios.
- ASan/UBSan Debug CTest: 5/5 groups pass, no reported runtime findings.
- Full Release regression: 75/75, 500,000 symbols/window; parsed CSV equals
  `docs/evidence/rework-regression.csv` from the working implementation.
- Replay fixture: both valid windows report MSE 0.0733642578125 and margin
  0.988052368164, identical measurements yes, exit 0.

Commands (from this solutions worktree):

```sh
cmake -S . -B /tmp/serdes-practice-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/serdes-practice-build -j 2
ctest --test-dir /tmp/serdes-practice-build --output-on-failure -V
python3 python/run_regression.py --executable /tmp/serdes-practice-build/serdes_lab \
  --seeds 25 --verify-symbols 500000 --output /tmp/serdes-solutions-full
cmake -S . -B /tmp/serdes-solutions-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DSERDES_ENABLE_SANITIZERS=ON
cmake --build /tmp/serdes-solutions-sanitize -j 2
ctest --test-dir /tmp/serdes-solutions-sanitize --output-on-failure
```

The same local WSL2 toolchain and non-fatal linker `.sframe` diagnostic described
in `docs/validation.md` apply. These are instructor measurements, not evidence
that the learner investigated or fixed either defect. Build separate hypotheses
and stories from their actual notes later.
