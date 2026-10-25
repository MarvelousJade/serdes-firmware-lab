# serdes-firmware-lab: impact and resume evidence

Reviewed October 25, 2026. [Source-review fingerprints](evidence/resume-review-2026-10-25.json).

## Existing evidence and candidate claim

**Status: preliminary for current-version reuse; historical engineering result.** This is a host-side behavioral controller/simulator, not a physical SERDES implementation. The useful outcome is bounded bring-up/BER acceptance under defined synthetic channels.

- Problem/change: train CTLE/DFE controls with bounded faults, then verify the trained simulated channel rather than accepting zero observed errors alone.
- Recorded result: 75/75 synthetic scenarios passed across three profiles and seeds 1–25, using 500,000-symbol baseline and trained windows. This is 75 million combined simulated decisions, not hardware throughput. The raw CSV retains every scenario, including baseline zero-error cases; never divide by a zero BER baseline.
- Scope: Release WSL2 run, GCC 14.2.1, CMake 4.0.1, Python 3.14.7, Linux 6.18.40.1; see [validation](validation.md) and [raw matrix](evidence/rework-regression.csv). The report lists Release and sanitizer regression groups separately; do not add overlapping totals.
- Baseline versus trained within each scenario is a control-state comparison, not two source revisions. Exact historical measured SHA, CPU/RAM, date and complete run-level logs are not established by the CSV alone. Current review hashes are in the manifest; historical evidence needs a matched current build before an unqualified current-version claim.
- Correctness: all success/fault/exit fields must agree, trained upper estimates must satisfy the documented 1e-3 acceptance threshold, and learned/reference tap differences remain bounded. The analytical reference uses the same simplified behavioral equations; it is not independent silicon validation. Zero observed errors does not prove zero BER.
- Personal contribution: not established by the automated matrix. Implementation and personal investigation must be confirmed separately.
- Conditional draft: “Verified CTLE/DFE bring-up across 75 synthetic channel/seed scenarios with a bounded BER acceptance check in a C++20 host-side simulator.”
- Safe capability wording now: “Implements bounded CTLE/DFE training, timeout/fault states and BER checks in a host-side C++20 simulator.”

## Next measurement and acceptance criteria

Run the documented clean Release build and regression on current source, retaining the exact tool versions and full output:

```sh
cmake -S . -B build/evidence -DCMAKE_BUILD_TYPE=Release
cmake --build build/evidence
ctest --test-dir build/evidence --output-on-failure -V
python python/run_regression.py --executable build/evidence/serdes_lab --seeds 25 --verify-symbols 500000 --output artifacts/evidence-current
```

Use the platform-specific executable path on Windows. Record source SHA, compiler flags, dataset and CSV hashes. Deterministic correctness needs case-level results, not repeated timing statistics; if claiming runtime changes, separately repeat matched timing trials at least five times. No new firmware runtime experiment was performed in this documentation increment. Strongest results remain synthetic bring-up acceptance and bounded timeout/failure behavior; no workflow or real-world impact exists.

## Measurement policy for future changes

Choose one or two useful metrics during planning and record acceptance criteria before implementation. Measure the old version first; for new functionality use an initial implementation or a defined manual workflow. Compare equivalent inputs, environments and required outputs. Without a defensible comparator, report an absolute result. Test totals, lines of code and commits are verification/scope evidence, not user impact.

For every experiment record metric/value, rationale, baseline and delivered source commits (or explicitly unknown/not applicable), date, OS, CPU/RAM, tool versions, dataset origin/hash/size and whether synthetic/demo/real, exact commands, repetitions, warm-up and cache state, all raw results, summary statistics and correctness checks. Record limitations, confounders and unfavorable runs. Repeat timings enough to distinguish variation; normally start with at least five paired trials and increase if variation warrants it. Keep cold and warm cache comparisons labeled and separate from code improvements.

Reduction = (baseline - improved) / baseline * 100. Speedup = baseline time / improved time. Use percentage points for differences between rates. Do not average incompatible workloads, mix source revisions, select only favorable samples or turn a reciprocal of latency into sustained throughput.

Separate engineering results (performance/correctness/recovery), workflow results (observed task time/manual steps) and real-world impact (observed adoption/user/business outcomes). Synthetic experiments support engineering claims only. Do not invent hours saved, customers, revenue or production capacity.

Each candidate claim must record problem/baseline, change, measured result, supporting commits/artifacts, conditions/limitations, verified personal contribution, proposed bullet and status **verified**, **preliminary** or **unverified**. Project results do not establish who personally implemented or executed the work. Use first-person implementation verbs only when contribution is independently known; otherwise use the capability wording below. Record confirmed personal work separately.

Before handoff trace every number to evidence, check applicability to the delivered tree, identify the strongest two or three results and mark remaining validation. Prepare draft bullets here; change an actual resume only when requested under the current resume instructions.

### Record template

| Field | Required record |
| --- | --- |
| Problem / value / category | Why this metric matters; engineering, workflow or real-world |
| Baseline / improved revision | Full SHAs; dirty state and source hashes; use N/A if no comparator |
| Environment / date | OS, CPU/RAM, runtime/compiler/dependency versions |
| Dataset | Origin, hash, size, characteristics; synthetic/demo/real |
| Procedure | Exact commands/steps; repetitions; warm-up/cache; condition order |
| Raw evidence / summary | Every sample/artifact; median/range/p95 as appropriate |
| Arithmetic / correctness | Explicit calculation; equivalent required outputs |
| Limitations | Noise, missing metadata, uncontrolled factors, negative results |
| Contribution / bullet / status | Actual personal work, supported wording, validation still needed |
