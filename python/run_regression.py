#!/usr/bin/env python3
"""Run deterministic C++ link-training scenarios and write reviewable results."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import statistics
import subprocess
import sys

from reference_model import CHANNELS, ideal_dfe_codes


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--seeds", type=int, default=20)
    parser.add_argument("--verify-symbols", type=int, default=200_000)
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("artifacts"))
    parser.add_argument("--profiles", nargs="+", default=list(CHANNELS))
    return parser.parse_args()


def run_case(executable: pathlib.Path, profile: str, seed: int, symbols: int) -> dict:
    command = [
        str(executable),
        "--profile",
        profile,
        "--seed",
        str(seed),
        "--verify-symbols",
        str(symbols),
        "--json",
    ]
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if not lines:
        raise RuntimeError(f"no JSON from {' '.join(command)}: {completed.stderr.strip()}")
    result = json.loads(lines[-1])
    result["return_code"] = completed.returncode
    ideal = ideal_dfe_codes(CHANNELS[profile], result["ctle_code"])
    result["ideal_dfe_tap_codes"] = list(ideal)
    result["max_tap_code_error"] = max(
        abs(actual - expected)
        for actual, expected in zip(result["dfe_tap_codes"], ideal)
    )
    return result


def write_csv(path: pathlib.Path, rows: list[dict]) -> None:
    fields = [
        "profile",
        "seed",
        "success",
        "fault",
        "baseline_errors",
        "baseline_symbols",
        "baseline_ber",
        "trained_errors",
        "trained_symbols",
        "trained_ber",
        "trained_ber_95_upper",
        "ctle_code",
        "dfe_tap_codes",
        "ideal_dfe_tap_codes",
        "max_tap_code_error",
        "training_windows",
        "return_code",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def write_summary(path: pathlib.Path, rows: list[dict]) -> None:
    passed = sum(bool(row["success"]) for row in rows)
    baseline = statistics.median(row["baseline_ber"] for row in rows)
    conservative_trained = statistics.median(row["trained_ber_95_upper"] for row in rows)
    improvement = baseline / conservative_trained if conservative_trained > 0 else float("inf")
    worst_tap_error = max(row["max_tap_code_error"] for row in rows)
    content = (
        "# Regression summary\n\n"
        f"- Passing scenarios: {passed}/{len(rows)} ({passed / len(rows):.1%})\n"
        f"- Median baseline BER: {baseline:.3e}\n"
        f"- Median trained BER, conservative value: {conservative_trained:.3e}\n"
        f"- Conservative median improvement: {improvement:.1f}x\n"
        f"- Worst learned-to-reference tap-code difference: {worst_tap_error}\n"
        f"- Symbols checked per baseline/trained window: {rows[0]['trained_symbols']:,}\n"
    )
    path.write_text(content, encoding="utf-8")
    print(content, end="")


def main() -> int:
    args = parse_args()
    if args.seeds <= 0 or args.verify_symbols <= 0:
        raise SystemExit("--seeds and --verify-symbols must be positive")
    unknown = set(args.profiles) - set(CHANNELS)
    if unknown:
        raise SystemExit(f"unknown profiles: {', '.join(sorted(unknown))}")

    executable = args.executable.resolve()
    rows = [
        run_case(executable, profile, seed, args.verify_symbols)
        for profile in args.profiles
        for seed in range(1, args.seeds + 1)
    ]
    args.output.mkdir(parents=True, exist_ok=True)
    write_csv(args.output / "regression.csv", rows)
    write_summary(args.output / "summary.md", rows)
    return 0 if all(row["success"] for row in rows) else 1


if __name__ == "__main__":
    sys.exit(main())

