"""Small floating-point reference helpers for the C++ PHY model."""

from __future__ import annotations

from dataclasses import dataclass


CTLE_FEEDBACK = (0.00, 0.12, 0.22, 0.32, 0.42, 0.50, 0.58, 0.65)
TAP_LSB = 1.0 / 64.0
TAP_MIN = -63
TAP_MAX = 63


@dataclass(frozen=True)
class Channel:
    impulse: tuple[float, ...]
    noise_sigma: float


CHANNELS = {
    "short": Channel((1.0, 0.24, -0.06, 0.02), 0.11),
    "medium": Channel((1.0, 0.62, -0.30, 0.14), 0.16),
    "long": Channel((1.0, 0.82, -0.48, 0.26), 0.20),
}


def effective_impulse(impulse: tuple[float, ...], ctle_code: int) -> tuple[float, ...]:
    """Apply y[n] - r*y[n-1] to a causal channel impulse response."""
    feedback = CTLE_FEEDBACK[ctle_code]
    result = [impulse[0]]
    result.extend(
        impulse[index] - feedback * impulse[index - 1]
        for index in range(1, len(impulse))
    )
    result.append(-feedback * impulse[-1])
    return tuple(result)


def quantize_tap(value: float) -> int:
    return max(TAP_MIN, min(TAP_MAX, round(value / TAP_LSB)))


def ideal_dfe_codes(channel: Channel, ctle_code: int, tap_count: int = 3) -> tuple[int, ...]:
    effective = effective_impulse(channel.impulse, ctle_code)
    main_cursor = effective[0]
    return tuple(quantize_tap(effective[index] / main_cursor) for index in range(1, tap_count + 1))


def zero_error_ber_upper_bound_95(symbols: int) -> float:
    """Rule-of-three upper bound after observing zero errors."""
    if symbols <= 0:
        raise ValueError("symbols must be positive")
    return 3.0 / symbols

