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
