# Agent investigations and main integration

These defects were deliberately introduced for learning, not discovered in
production or in the correct rework baseline. Reports and possible consequences
in `debugging-exercises.md` are invented practice material. The investigation and
measurements below were performed by the agent; they do not establish the user's
personal debugging experience.

## History and integration decision

Initial refs: `main` = `37f837f`; `shaoyu/rework` = `85a5eb1`;
`shaoyu/learning` = `39c6df5`; `shaoyu/practice-a` = `fd1b707`;
`shaoyu/practice-b` = `39c6df5`; `shaoyu/learning-solutions` = `0ea67cd`.
Rework and solutions diverge at `aefbd7d`. Solutions contains two focused fixes:
A `19c8e6c`, then B `0ea67cd`. Their corrected controller/model contents already
match rework, so cherry-picking would duplicate equivalent commits and omit the
exercise ancestry. Chosen: fast-forward main to rework, then merge solutions
without squash. The original checkpoint tags remain unchanged. After integration,
redundant learning/solutions branch labels and the temporary learning worktree
were removed; fixes remain in main and faulty versions remain in the tags.

All work used separate verification/integration worktrees. The original checkout
remains on `shaoyu/rework` with its unstaged edits untouched. Main's merge keeps
the useful reproduction fixtures as regressions under `BUILD_TESTING`; the
ordinary CLI and library architecture are unchanged. No conflicts occurred.
No new equivalent bug-fix commits, resets, history rewrites, or pushes were made.

## A — consecutive-failure state

**Symptom/reproduction:** after successful short-channel bring-up, apply bad,
bad, healthy, bad, bad, bad offline windows. At `39c6df5`, the fourth window
returns RetrainRequired instead of Observe; later checks return NotLinkUp.
`serdes_practice health` exits 1. Existing `test_degradation_hysteresis` fails
three action assertions, independently of the learning fixture.

**Investigation:** traced `check_link_health()` and its persistent counter. Bad
windows increment it and the healthy path returns without clearing it. Thus
bad/bad/healthy leaves a count of two; the next bad window reaches the threshold
of three. That changes state to Degraded and explains subsequent NotLinkUp
without invoking PLL or BER-model failure. The fixture's healthy action confirms
the intervening window was recognized as healthy, not silently rejected.

**Alternatives:** reset on bring-up only (does not handle intermittent recovery);
raise the threshold (hides symptoms, changes policy); use a moving average or
time-decayed score (reasonable different policy but unnecessary scope). Chosen:
restore the reset on every accepted healthy window. It directly implements
"consecutive" and keeps immediate clock/invalid-measurement faults unchanged.

**Fix/evidence:** reused existing `19c8e6c`, a one-line behavioral fix plus its
explanation, rather than creating an equivalent commit. After rebuilding at
that commit, the six expected actions all pass. CTest passes 3/5 groups; only B
and its 24 replay assertions remain failing. Before this fix it passed 2/5 groups
with 27 C++ failures. See [before](evidence/integration/before.txt) and
[after A](evidence/integration/after-a.txt).

## B — complete deterministic restart

**Symptom/reproduction:** restart seed 42 before each 1,025-symbol verification
window on the short channel. Both windows are valid with zero bit errors, but
MSE changes from 0.0733642578125 to 0.0772094726562 and margin from 0.988052368164
to 0.982009887695. `serdes_practice replay` exits 1; 24 existing repeated-window
comparisons fail across profiles/modes/seeds.

**Investigation:** followed `step_symbol()` through `next_gaussian_sample()`.
Box–Muller generates two samples and retains the second with a validity flag.
The restart reset PRBS, uniform PRNG and channel/feedback histories, but not the
Gaussian cache. With 32 warmup plus 1,025 measured symbols, an odd number of
samples leaves that flag set. The repeat consumes an old sample before the new
PRNG stream, shifting its relationship to the data and history. Seed equality
alone therefore does not imply complete state equality.

**Alternatives:** remove caching (simpler but performs more transforms and changes
the sampled stream); use only even windows (hides an API defect); zero only the
cached value (a still-valid zero sample is also wrong); loosen float comparisons
(hides state drift). Chosen: clear the cached value and invalidate it at restart,
while preserving taps and PLL state. Restarting the sequence is not device reset.
These alternatives were evaluated by code reasoning, not benchmarked experiments.

**Fix/evidence:** reused `0ea67cd`, the focused cache-reset fix. Rebuilt at that
commit: both windows now have MSE 0.0733642578125 and margin 0.988052368164;
all measurement fields match and the fixture exits 0. CTest passes 5/5 groups,
including all 187 C++ checks, both fixtures, 12 Python tests and 6 smoke scenarios.
See [after B](evidence/integration/after-b.txt). No tests were weakened.

## Reproduce the before/after checkpoints

From a separate worktree (do not discard unrelated modifications):

```sh
git worktree add --detach ../serdes-check shaoyu/practice-b
cd ../serdes-check
cmake -S . -B /tmp/serdes-check-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/serdes-check-build -j 2
ctest --test-dir /tmp/serdes-check-build --output-on-failure
# Expected failures: both fixtures and 27 C++ checks.
git switch --detach 19c8e6c
cmake --build /tmp/serdes-check-build -j 2
ctest --test-dir /tmp/serdes-check-build --output-on-failure
# Health passes; replay and 24 C++ checks still fail.
git switch --detach 0ea67cd
cmake --build /tmp/serdes-check-build -j 2
ctest --test-dir /tmp/serdes-check-build --output-on-failure
# All five groups pass.
```

Always rebuild after switching: an old executable is not evidence for a new
checkpoint. Actual agent verification used `../serdes-exercise-verification`
and `/tmp/serdes-exercise-check` with this same sequence.

## Integrated main verification

From `../serdes-main-integration`, containing the merged sources:

```sh
cmake -S . -B /tmp/serdes-main-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/serdes-main-release -j 2
ctest --test-dir /tmp/serdes-main-release --output-on-failure -V
/tmp/serdes-main-release/serdes_tests
python3 -m unittest discover -s python/tests -p 'test_*.py'
python3 python/run_regression.py --executable /tmp/serdes-main-release/serdes_lab \
  --seeds 25 --verify-symbols 500000 --output /tmp/serdes-main-full
cmake -S . -B /tmp/serdes-main-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DSERDES_ENABLE_SANITIZERS=ON
cmake --build /tmp/serdes-main-sanitize -j 2
ctest --test-dir /tmp/serdes-main-sanitize --output-on-failure -V
```

Actual results on GCC 14.2.1, CMake 4.0.1, Python 3.14.7, WSL2 Linux:
- Release and ASan/UBSan Debug: 5/5 groups pass; 187 C++ checks, both targeted
  fixtures, 12 Python tests, 6 real CLI smoke scenarios. No sanitizer findings.
- Direct C++ and Python runs also pass, and manual medium/seed-42 CLI passes:
  CTLE 4, taps [12,-36,17], 17 training windows, 0/200,000 trained errors.
- Full Release matrix: 75/75, three profiles × seeds 1–25, 500,000 symbols per
  baseline/trained window. All parsed CSV rows equal `rework-regression.csv`.
  54/75 zero-error trained windows; worst trained BER 5e-5, upper estimate
  6.937e-5, maximum reference-tap difference 1.
- `BUILD_TESTING=OFF` Release builds and runs the short-channel CLI; no practice
  executable is produced. Fixtures remain test infrastructure, not a new product.

Logs: [Release](evidence/integration/main-release.txt),
[sanitizers](evidence/integration/main-sanitize.txt). The toolchain emits the
previously documented non-fatal `.sframe` linker diagnostic; build exits are zero
and binaries run. No Windows/MSVC, remote CI, production or physical-hardware
verification is claimed. Synthetic finite-window evidence, synchronous operation,
caller-selected extreme budgets and the runner's lack of wall-clock timeout
remain limitations.
