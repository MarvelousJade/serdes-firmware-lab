# Rework interview preparation

Describe this as maintenance of an existing software prototype, not as fresh
original development or deployed hardware experience. The following is a
project explanation, not a claim about the learner's personal work.

## 60-second introduction

This lab explores how firmware can start and tune a simulated serial receiver.
A C++ controller operates through a typed register driver; a software PHY models
the signal and reports measurements. Startup resets the receiver, waits for
clock readiness, compares eight front-end settings, trains three feedback taps,
and checks decision-directed bit errors. Acceptance uses a finite-sample upper
estimate rather than treating zero observed errors as proof of perfection.

The rework retained those useful boundaries instead of rebuilding the project.
It added policy validation, fresh health-result semantics, recovery tests, and
checks that the Python regression runner's JSON agrees with process status and
requested samples. Local Release and sanitizer tests passed, as did 75 synthetic
scenarios whose outputs match the earlier baseline. This is a synchronous host
prototype, not calibrated silicon firmware. Faulty learning checkpoints remain
separate; corrected fixtures and the individual fix commits are now integrated
into main.

## Three-minute technical walkthrough

**0:00–0:45 — follow one workflow.** Start at `app/main.cpp`: arguments select
profile/seed/window length; stack-owned model, driver, and controller are wired
in that order. `bring_up()` validates policy before I/O and records fixed-size
state transitions. Dependencies must outlive the objects referencing them.

**0:45–1:30 — adaptation and evidence.** After bounded PLL polling, baseline BER
is measured. The CTLE sweep restarts identical PRBS/noise/history for each
candidate, choosing fewer errors then lower MSE. DFE training uses known-symbol
feedback and bounded sign-correlation updates with saturation and a deadband.
Stable taps are not proof of good BER: separate verification uses receiver
decisions and a distinct seed. Explain the rule-of-three/Wilson approximation
and why correlated decision errors limit its statistical interpretation.

**1:30–2:15 — failure and recovery.** Explicit faults distinguish invalid policy,
PLL timeout, measurement fault, non-convergence, and missed target. Offline health
windows reset their failure counter when healthy and request retraining after
three consecutive bad windows. Invalid measurements/clock loss fault immediately.
A saved health window is invalid after bring-up or a skipped check, avoiding
stale current-looking evidence. The caller initiates recovery; no RTOS scheduler
or background live-traffic monitor is implemented.

**2:15–3:00 — verification and tradeoffs.** Show the regression tests, reproducible
commands and CSV in `validation.md`. Distinguish 187 C++ checks, 12 Python tests,
6 smoke scenarios, and 75 full synthetic scenarios. Explain why a short predicate
and two state resets beat exceptions, a generic validator, or timestamped
telemetry for this scope. Explain exit/JSON agreement in the Python runner. State
limits: synthetic channel, matching-equation reference, synchronous execution,
no physical timing/transport/PVT validation. See `rework.md` for alternatives.

## Completed agent investigations — concise explanations

**State consistency:** the practice link requested recovery after non-consecutive
failures. The agent traced the six-window fixture and healthy return path: the
counter was retained across recovery. Restoring its reset implements the existing
policy without raising thresholds. Fix `19c8e6c` passes the health fixture while
the independent replay defect remains observable.

**Cached random state:** same-seed odd windows produced different metrics with
zero bit errors. Inspection showed that restart reset the PRNG but not the cached
second Gaussian sample. Fix `0ea67cd` invalidates that cache; exact full-field
replay and all five integrated test groups pass. Comparing only BER or loosening
float tolerances would hide the problem.

These are explanations of actual **agent** work on intentionally faulty
checkpoints, not personal experience stories for the user. Commands, alternatives
and measured evidence are in [investigations](debugging-investigations.md).

## If you investigate the exercises yourself

Keep notes containing:
1. Observed symptom and exact checkpoint/command.
2. Initial hypotheses, including ones disproved.
3. Evidence (assertions, outputs, inspected state); distinguish speculation.
4. Chosen change and rejected alternatives.
5. Regression that failed before and passed after, plus broader checks.
6. Remaining uncertainty and what you actually learned.

Only then turn your notes into **Problem → hypothesis → evidence → decision →
fix → verification** stories. No learner experience story is invented here.

Follow-up questions:
- Why are consecutive failures different from lifetime failures?
- Which state must restart for deterministic replay, and which must survive?
- Why train with known bits but verify with decisions?
- What does a 500,000-symbol error-free sample actually support?
- Why check both process status and JSON? What if old artifacts still exist?
- How would real asynchronous hardware change polling and coherent reads?
- When would generation IDs or historical telemetry be preferable to invalidation?
- Which assertions check behavior rather than implementation details?
