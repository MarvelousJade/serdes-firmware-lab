# Architecture

## Responsibility boundary

`FirmwareController` owns synchronous sequencing and policy. It resets the device, applies bounded timeouts, selects CTLE and DFE settings, checks a confidence-aware BER acceptance target, and applies hysteresis to offline PRBS/BERT health windows. It depends only on `PhyDriver` and therefore has no knowledge of the channel implementation. It is a host-side firmware algorithm prototype, not a non-blocking production RTOS task.

`PhyDriver` is a typed hardware-abstraction layer over `IRegisterIo`. It masks control fields, encodes signed tap registers, starts measurement windows, polls completion, and converts fixed-point metrics. A real MMIO, SPI, or mailbox backend can replace the simulator without changing the controller.

`SimulatedPhy` is the behavioral hardware backend. It generates the line-rate PRBS symbols, applies channel and receiver transforms, makes slicer decisions, and accumulates BER, error power, margin, and tap correlations. Firmware sees only the resulting registers.

## Signal model

The source emits NRZ symbols `x[n]` in `{-1, +1}`. A four-cursor causal channel and additive noise produce:

```text
y[n] = sum(h[k] * x[n-k], k=0..3) + w[n]
```

Each CTLE code selects a coefficient `r` for a simple discrete high-frequency boost:

```text
z[n] = y[n] - r * y[n-1]
```

This is a deliberately small behavioral approximation of a tunable analog front end, not a circuit model. It creates an explicit equalization/noise tradeoff that firmware can sweep.

The DFE subtracts three previous feedback decisions using signed register values `b[k]`:

```text
u[n] = z[n] - sum(b[k] * d[n-k], k=1..3)
```

Tap registers cover codes `[-63, 63]` with an LSB of `1/64`. During supervised training, `d` comes from the known PRBS pattern. During verification and monitoring, it comes from slicer decisions.

For a training window, the PHY exposes sign-sign error correlations:

```text
C[k] = sum(sign(x[n] - u[n]) * d[n-k])
```

Firmware applies a one-, two-, or three-code bounded update opposite the normalized correlation. A deadband suppresses noise-driven movement, and three unchanged windows declare convergence; exhausting the configured window budget is a distinct fault. The Python reference analytically applies the same CTLE difference equation to the known channel impulse response, then quantizes the ideal first three post-cursors. This is a cross-language consistency check against matching equations, not independent physical validation.

## Control flow

1. `RESET`: clear CTLE/DFE state and reset the simulated PHY.
2. `WAIT_FOR_PLL`: poll the lock bit for at most 32 deterministic ticks.
3. Baseline: measure the unequalized channel over the configured verification length.
4. `CTLE_SWEEP`: replay an identical seeded PRBS/noise window for all eight codes; minimize errors, using mean squared error as a tie-breaker.
5. `DFE_TRAINING`: consume block-level correlations and update saturated tap codes.
6. `VERIFY`: restart with a distinct deterministic seed and measure decision-directed BER.
7. `LINK_UP`: accept only if the approximate one-sided 95% BER upper estimate is at or below `1e-3`.
8. `DEGRADED`: require three consecutive failing offline BERT windows before requesting retraining.

Every measurement uses a bounded symbol count and returns an explicit validity flag. PLL and measurement failures lead to `FAULT`; a missed BER target produces a distinct fault reason.

## Determinism

PRBS, channel history, and noise state restart from the programmed pattern seed. Noise uses a local xorshift generator and a Box–Muller transform. This is sufficient for reproducible functional regression but is not a calibrated statistical communications or compliance simulator.

The controller stores its state trace in a fixed-size array. The register path uses fixed-width integers and saturating codes. CMake separates `serdes_firmware_core` from `serdes_behavioral_model`, but the controller still uses floating-point host metrics and a virtual I/O interface. The project should therefore be described as embedded-style host prototype firmware rather than deployable bare-metal firmware.
