# Debugging practice (learner instructions — no solutions)

All reports and consequences below are **invented practice material**, not real
user incidents or production measurements. Reproduction outputs on faulty
checkpoints are actual local observations recorded separately. Do not merge a
learning checkpoint into the working branch.

Working branch: `shaoyu/rework`. Faulty branch: `shaoyu/learning`.
Use a separate worktree so your current uncommitted work stays untouched:

```sh
git worktree add --detach ../serdes-practice shaoyu/learning
cd ../serdes-practice
git switch -c shaoyu/practice-investigation
cmake -S . -B /tmp/serdes-learner-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/serdes-learner-build -j 2
/tmp/serdes-learner-build/serdes_practice health
/tmp/serdes-learner-build/serdes_practice replay
ctest --test-dir /tmp/serdes-learner-build --output-on-failure
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

**Hypothetical consequence:** equalizer candidate comparisons or regression
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

## Preserved checkpoints and actual observations

| Checkpoint | Ref | Actual local result |
|---|---|---|
| A | `fd1b707` / `shaoyu/practice-a` | 3/5 CTest groups pass; health fixture exits 1; 3 C++ assertions fail |
| B (A+B) | `39c6df5` / `shaoyu/practice-b` / `shaoyu/learning` | 2/5 groups pass; both fixtures exit 1; 27 C++ assertions fail |

Full symptom outputs are in `learning/observations.md` on the learning branch.
Python's 12 tests and the 6 ordinary startup smoke cases pass at both faulty
checkpoints. Targeted behavioral tests therefore matter even when startup works.

Prepared solutions on `shaoyu/learning-solutions` pass all 5 Release and
ASan/UBSan CTest groups (187 C++ checks, both fixtures, 12 Python tests, 6 smoke
cases), plus the 75-case full regression matching the working CSV. Those are
instructor verification results, not learner accomplishments. Your investigation,
fixes, and interview stories remain pending.

An agent-created worktree is also available at `../serdes-learning-worktree`,
checked out on the faulty learning branch. Use a **fresh build** or rebuild after
changing checkpoints; an old executable may still contain a prepared solution.
The detached-worktree command above allows independent investigation without
changing that worktree or the working implementation.
