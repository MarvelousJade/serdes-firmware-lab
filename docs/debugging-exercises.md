# Debugging practice (learner instructions — no solutions)

All reports and consequences below are **invented practice material**, not real
user incidents or production measurements. Reproduction outputs on faulty
checkpoints are actual local observations recorded separately. Do not merge a
learning checkpoint into the working branch.

Working branch: `shaoyu/rework`. Faulty branch: `shaoyu/learning`.
Use a separate worktree so your current uncommitted work stays untouched:

```sh
git worktree add ../serdes-practice shaoyu/learning
cd ../serdes-practice
cmake -S . -B /tmp/serdes-practice-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/serdes-practice-build -j 2
/tmp/serdes-practice-build/serdes_practice health
/tmp/serdes-practice-build/serdes_practice replay
ctest --test-dir /tmp/serdes-practice-build --output-on-failure
```

On Windows choose your own out-of-tree build path and use the configuration's
`.exe` path. `serdes_practice` is a learning-only executable, not a production CLI
feature. The branch intentionally fails selected checks. Both exercises also
have existing C++ regression coverage; do not weaken assertions to make it pass.

## Exercise A — intermittent recovery requests

**Simulated report:** “A link that recovers between short disturbances still asks
for retraining earlier than expected.”

**Hypothetical consequence:** unnecessary recovery could interrupt otherwise
usable service. This lab tests offline windows, not production traffic.

Reproduce with `serdes_practice health`. The fixture brings up a short channel,
uses two bad windows, restores one healthy window, then applies another
disturbance. Compare the action with the policy of three consecutive bad
windows. Checkpoint tag: `shaoyu/practice-a` (only this defect). The final learning
branch includes this and Exercise B. Investigate observations, state transitions,
and recovery behavior; keep a log of competing hypotheses.

## Exercise B — inconsistent repeated measurement

**Simulated report:** “Repeating the same seeded channel measurement sometimes
changes metrics, especially after an odd-sized window.”

**Hypothetical consequence:** unequalizer candidate comparisons or regression
results could depend on prior activity instead of settings alone.

Reproduce with `serdes_practice replay`. The fixture locks the short channel,
restarts seed 42 before each 1,025-symbol verification window, and compares all
measurement fields exactly. Checkpoint tag: `shaoyu/practice-b` (both defects).
A restart with the same seed and settings should reproduce the same measurement.
Do not change floating-point tolerances just to hide a state discrepancy.

## Investigation rules

- Start with the symptoms and fixtures, not a solution diff.
- Record commands, checkpoint hash, observed output, and hypotheses separately.
- Add/refine a failing behavioral regression before fixing the implementation.
- Keep the reproduction checkpoints intact; commit fixes on your own branch.
- Run the targeted fixture, existing C++ tests, Python tests, and real smoke cases
  after each fix. Check both healthy behavior and failure/recovery paths.
- Ask for “A hint 1” or “B hint 1” for progressive guidance. No hints are embedded
  here. Diagnosis, progressive hints, and verified solution records are kept only
  on `shaoyu/learning-solutions`; avoid opening it until you are ready.
- Afterward, share your notes for interview stories grounded in your actual work.

The instructor verified prepared fixes separately; that does not mean the learner
has investigated or solved either exercise yet.
