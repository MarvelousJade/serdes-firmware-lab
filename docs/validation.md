# Validation record

## Current local verification — 2026-09-08

The cleanup uses a direct nonzero noise seed and clearer names. The older record below describes the previous initialization and is retained as historical evidence. Changing the seed initialization changes sampled results; it does not change the signal equations or acceptance policy.

Completed on Windows with GCC 13.2.0, C++20, `-O3 -DNDEBUG`, the repository warning flags, and Python 3.12.10:

- C++ test executable: **104 checks passed**. This is the original 50 checks plus 54 assertions covering sequence replay and zero-seed fallback across profiles, modes, and seeds.
- Python unit tests: **5 passed**.
- Regression smoke: **6/6 scenarios passed**, using 50,000-symbol windows.
- Full regression: **75/75 scenarios passed**, using three profiles, seeds 1–25, and 500,000 symbols in each baseline and verification window.
- All 75 complete regression rows matched the separately tested direct-seed implementation before the naming cleanup.

The replay tests use odd-length windows to leave a cached noise sample, then restart and compare all measurement fields. They exercise training and verification modes with seeds 0, 1, 42, and 0xFFFFFFFF on short, medium, and long channels. Zero is checked against the nonzero default seed.

The executables were compiled directly from the six library sources and the respective app/test entry point. CMake/Ninja compiler detection stalled in this local sandbox, so this record does not claim a completed CTest run or a new remote CI/sanitizer run. The CMake configuration is unchanged.

Reproduction from a GCC-equipped checkout in PowerShell:

```powershell
New-Item -ItemType Directory -Force -Path build | Out-Null
$sources = @('src/firmware_controller.cpp', 'src/phy_driver.cpp', 'src/prbs31.cpp', 'src/statistics.cpp', 'src/channel_profiles.cpp', 'src/simulated_phy.cpp')
$flags = @('-std=c++20', '-O3', '-DNDEBUG', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-Iinclude')
g++ @flags @sources app/main.cpp -o build/serdes_lab.exe
g++ @flags @sources tests/serdes_tests.cpp -o build/serdes_tests.exe
./build/serdes_tests.exe
python -m unittest discover -s python/tests -p 'test_*.py'
python python/run_regression.py --executable build/serdes_lab.exe --seeds 2 --verify-symbols 50000 --output artifacts/smoke
python python/run_regression.py --executable build/serdes_lab.exe --seeds 25 --verify-symbols 500000 --output artifacts
```

| Metric | Result |
|---|---:|
| Successful bring-ups | 75/75 |
| Trained windows with zero observed errors | 54/75 |
| Median baseline bit error rate | 9.147e-2 |
| Maximum observed trained bit error rate | 5.000e-5 |
| Maximum approximate 95% upper estimate | 6.937e-5 |
| Worst learned/reference tap-code difference | 1 |

The acceptance threshold remains an approximate 95% upper estimate at or below 1e-3. The [full CSV](evidence/2026-09-08-seed-cleanup.csv) records each scenario. These checks establish behavior inside the software model; they do not establish equal statistical quality between seed schemes, independence of noise streams, or physical hardware performance.

## Historical record — 2026-07-16

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
