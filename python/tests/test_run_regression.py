import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import run_regression


class RegressionRunnerTests(unittest.TestCase):
    def response(self, success=True, return_code=0):
        payload = {
            "profile": "short", "seed": 1, "success": success,
            "ctle_code": 0, "dfe_tap_codes": [15, -4, 1],
            "baseline_symbols": 50000, "trained_symbols": 50000,
        }
        return payload, subprocess.CompletedProcess([], return_code, json.dumps(payload), "")

    def test_successful_case_keeps_reference_comparison(self):
        _, response = self.response()
        with patch.object(run_regression.subprocess, "run", return_value=response):
            result = run_regression.run_case(pathlib.Path("lab"), "short", 1, 50000)
        self.assertEqual(result["return_code"], 0)
        self.assertEqual(result["max_tap_code_error"], 0)

    def test_success_with_nonzero_exit_is_not_evidence(self):
        _, response = self.response(return_code=2)
        with patch.object(run_regression.subprocess, "run", return_value=response):
            with self.assertRaises(RuntimeError):
                run_regression.run_case(pathlib.Path("lab"), "short", 1, 50000)

    def test_failure_with_zero_exit_is_inconsistent(self):
        _, response = self.response(success=False)
        with patch.object(run_regression.subprocess, "run", return_value=response):
            with self.assertRaises(RuntimeError):
                run_regression.run_case(pathlib.Path("lab"), "short", 1, 50000)

    def test_reported_bringup_failure_is_retained(self):
        _, response = self.response(success=False, return_code=2)
        with patch.object(run_regression.subprocess, "run", return_value=response):
            result = run_regression.run_case(pathlib.Path("lab"), "short", 1, 50000)
        self.assertFalse(result["success"])
        self.assertEqual(result["return_code"], 2)

    def test_success_must_match_requested_case(self):
        for field, value in (("profile", "long"), ("seed", 2),
                             ("baseline_symbols", 1), ("trained_symbols", 1)):
            with self.subTest(field=field):
                payload, response = self.response()
                payload[field] = value
                response.stdout = json.dumps(payload)
                with patch.object(run_regression.subprocess, "run", return_value=response):
                    with self.assertRaises(RuntimeError):
                        run_regression.run_case(pathlib.Path("lab"), "short", 1, 50000)

    def test_missing_output_is_not_evidence(self):
        response = subprocess.CompletedProcess([], 1, "", "input error")
        with patch.object(run_regression.subprocess, "run", return_value=response):
            with self.assertRaisesRegex(RuntimeError, "input error"):
                run_regression.run_case(pathlib.Path("lab"), "short", 1, 50000)

    def test_main_fails_a_failed_scenario(self):
        args = run_regression.argparse.Namespace(
            executable=pathlib.Path("lab"), profiles=["short"], seeds=1,
            verify_symbols=50000, output=pathlib.Path("unused"))
        with tempfile.TemporaryDirectory() as directory:
            args.output = pathlib.Path(directory)
            with patch.object(run_regression, "parse_args", return_value=args), \
                 patch.object(run_regression, "run_case", return_value={"success": False}), \
                 patch.object(run_regression, "write_csv"), \
                 patch.object(run_regression, "write_summary"):
                self.assertEqual(run_regression.main(), 1)


if __name__ == "__main__":
    unittest.main()
