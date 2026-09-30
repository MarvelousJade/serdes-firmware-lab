# SerDes Firmware Adaptation Lab

A C++20 software prototype for starting, adjusting, and testing a simulated high-speed data receiver. SerDes means serializer/deserializer: a system that converts between parallel data and a serial stream.

The project explores a practical control problem: how to choose receiver settings, check the result, and report failures without waiting forever. It runs on a computer with a simulated device, not a physical chip.

## How it works

```text
Application -> Controller -> Driver -> Register interface -> Simulated device
```

- **Controller:** decides which startup step to run and whether the result is acceptable.
- **Driver:** turns operations such as reset and measure into register reads and writes. Registers are device-facing locations for commands, settings, and status.
- **Simulated device:** models the signal and receiver, then exposes measurement results through those registers.

A successful startup resets the receiver, waits for clock readiness, compares receiver settings, trains correction settings, and checks the received data. A failed wait or measurement returns a failure instead of hanging. Separate health checks can request retraining; they do not perform it automatically.

The register interface is replaceable. The implemented device backend is the simulator; production hardware transport drivers have not been built.

## Build and run

Requirements: CMake 3.20+, a C++20 compiler, and Python 3.10+ for the Python tests and regression scripts.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run one scenario with a single-configuration Windows build:

```powershell
./build/serdes_lab.exe --profile medium --seed 42
```

With a Visual Studio multi-configuration build, use `./build/Release/serdes_lab.exe`. On Linux, use `./build/serdes_lab`; `-C Release` is unnecessary for a single-configuration generator.

To repeat the full set of scenarios, substitute your executable path if needed:

```powershell
python python/run_regression.py --executable build/serdes_lab.exe --seeds 25 --verify-symbols 500000
```

## Recorded evidence and limits

The September 8, 2026 local validation record reports **104 C++ checks, 5 Python tests, 6 smoke scenarios, and 75 full regression scenarios passing**. These are recorded results, not a claim that tests were rerun for this documentation update. Compiler details, measurements, earlier results, and reproduction commands are in the [validation record](docs/validation.md).

Tests cover behavior inside a simplified software model. Synthetic channels are test fixtures, not measured hardware. An error-free sample does not establish a zero underlying error rate. The model does not establish electrical performance, physical timing, or compliance with a hardware standard.

## Documentation

Start with this README. For detailed learning, use the [interview code guide](docs/interview-code-guide.md). Describe your own contribution accurately; a document is supporting material, not a script to recite.

| Reference | Open it when you need... |
|---|---|
| [Architecture](docs/architecture.md) | Receiver equations, control stages, or repeatability details |
| [Register map](docs/register-map.md) | Exact register addresses, fields, and access rules |
| [Validation](docs/validation.md) | Recorded test results, measurement conditions, or limitations |
| [Interview code guide](docs/interview-code-guide.md) | Detailed explanations of functions, variables, design tradeoffs, and questions to practise |
| [September 8 cleanup history](docs/cleanup-2026-09-08.md) | Earlier names and the seed-initialization change |

These references support deeper questions; they are not a required presentation order.

## Verified rework and learning

The `shaoyu/rework` branch preserves the existing workflow and adds policy
validation, current health evidence, recovery regressions, and consistent
CLI/JSON regression checks. Local Release and ASan/UBSan runs passed 187 C++
checks, 12 Python tests, and 6 smoke scenarios; the 75-case full regression
matches the historical baseline. See the [rework record](docs/rework.md),
[current verification](docs/validation.md#rework-verification--local-wsl2-run),
and [60-second / three-minute walkthroughs](docs/rework-interview.md).

Main now also contains the corrected learning history and individual fixes
`19c8e6c` and `0ea67cd`, integrated without squash. Both targeted fixtures pass
alongside the existing checks: Release and ASan/UBSan each pass 5/5 CTest groups;
the full regression still matches the historical baseline. Fixtures are built
only with `BUILD_TESTING` enabled.

Faulty checkpoints remain in the practice tags; redundant learning branch labels
have been removed. Start with
[spoiler-free exercises](docs/debugging-exercises.md), or read the completed
[agent investigations](docs/debugging-investigations.md) for causes and evidence.
These are practice scenarios, not production incidents or claims about your
personal debugging experience.

Licensed under the [MIT License](LICENSE).
