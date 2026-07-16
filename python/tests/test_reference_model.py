import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from reference_model import (  # noqa: E402
    CHANNELS,
    effective_impulse,
    ideal_dfe_codes,
    quantize_tap,
    zero_error_ber_upper_bound_95,
)


class ReferenceModelTests(unittest.TestCase):
    def test_zero_ctle_leaves_known_channel_taps(self) -> None:
        impulse = CHANNELS["medium"].impulse
        self.assertEqual(effective_impulse(impulse, 0)[:4], impulse)

    def test_ctle_difference_equation(self) -> None:
        effective = effective_impulse((1.0, 0.5), 5)
        self.assertEqual(effective, (1.0, 0.0, -0.25))

    def test_tap_quantizer_saturates(self) -> None:
        self.assertEqual(quantize_tap(10.0), 63)
        self.assertEqual(quantize_tap(-10.0), -63)

    def test_ideal_dfe_has_three_codes(self) -> None:
        self.assertEqual(len(ideal_dfe_codes(CHANNELS["long"], 4)), 3)

    def test_rule_of_three(self) -> None:
        self.assertAlmostEqual(zero_error_ber_upper_bound_95(1_000_000), 3e-6)
        self.assertEqual(zero_error_ber_upper_bound_95(1), 1.0)
        with self.assertRaises(ValueError):
            zero_error_ber_upper_bound_95(0)


if __name__ == "__main__":
    unittest.main()
