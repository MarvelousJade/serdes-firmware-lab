# Validation record

Validated locally on 2026-07-16 with GCC 13.2.0, CMake 4.0.3, Ninja 1.11.1, and Python 3.12.10 on Windows.

## Automated tests

```text
ctest --test-dir build --output-on-failure

1/3 serdes_cpp_tests ........... Passed
2/3 serdes_python_tests ........ Passed
3/3 serdes_regression_smoke .... Passed
100% tests passed
```

The dependency-free C++ test executable performs 50 checks covering:

- a fixed 64-bit PRBS31 signature and safe recovery from a zero seed;
- CTLE masking and signed DFE tap encode/decode/saturation;
- complete bring-up on short, medium, and long synthetic channels;
- improvement when the baseline channel has observed errors;
- driver measurement timeout and invalid-result handling;
- PLL timeout and explicit fault state;
- training-window exhaustion and non-convergence fault;
- zero- and nonzero-error BER upper-estimate semantics;
- three-window offline BERT degradation hysteresis and retrain request.

Python `unittest` checks the independent CTLE difference equation, ideal DFE tap calculation, tap saturation, and rule-of-three bound.

## Multi-seed regression

Command:

```powershell
python python/run_regression.py `
  --executable build/serdes_lab.exe `
  --seeds 25 `
  --verify-symbols 500000 `
  --output artifacts
```

Matrix:

- profiles: short, medium, long;
- deterministic seeds: 1 through 25;
- scenarios: 75;
- baseline symbols per scenario: 500,000;
- trained verification symbols per scenario: 500,000;
- total baseline plus trained decisions: 75,000,000;
- acceptance target: approximate one-sided 95% BER upper estimate at or below `1e-3`.

Results:

| Metric | Result |
|---|---:|
| Successful bring-ups | 75/75 |
| Trained windows with zero observed errors | 53/75 |
| Median baseline BER across all runs | 9.168e-2 |
| Maximum observed trained BER | 3.600e-5 |
| Maximum approximate 95% upper estimate | 5.292e-5 |
| Worst learned/reference tap-code difference | 1 |

For zero errors, the estimate uses the clamped rule of three (`3/N`). For nonzero errors it uses a one-sided 95% Wilson score estimate. These are finite-window approximations; an error-free run does not prove zero BER.

## Interpretation limits

These results establish deterministic behavior of this repository's simplified link model and controller. They do not predict silicon BER, compliance margin, analog performance, or behavior under timing jitter and process/voltage/temperature corners. The channel presets are synthetic test fixtures, not extracted PCB channels, and the Python tap calculation uses the same behavioral equations. CI configuration is checked in, but the table above records the local run only. The local MinGW toolchain lacks ASan/UBSan runtime libraries; sanitizer execution is configured for the Linux CI job but is not claimed in this local record.
