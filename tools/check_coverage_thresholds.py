#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path

_TOTAL_KEYS = ("LF", "LH", "BRF", "BRH", "FNF", "FNH")


def read_totals(path: Path) -> dict[str, int]:
    totals = dict.fromkeys(_TOTAL_KEYS, 0)
    seen: set[str] = set()

    for line in path.read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition(":")
        if not separator or key not in totals:
            continue
        try:
            number = int(value)
        except ValueError as error:
            raise ValueError(f"invalid {key} record: {line}") from error
        if number < 0:
            raise ValueError(f"negative {key} record: {line}")
        totals[key] += number
        seen.add(key)

    missing = [key for key in _TOTAL_KEYS if key not in seen]
    if missing:
        raise ValueError(f"missing LCOV records: {', '.join(missing)}")

    return totals


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Fail when LCOV coverage is below configured thresholds.")
    parser.add_argument("lcov", type=Path)
    parser.add_argument("--min-lines", type=float, default=100.0)
    parser.add_argument("--min-branches", type=float, default=85.0)
    parser.add_argument("--min-functions", type=float, default=100.0)
    args = parser.parse_args(argv[1:])

    for option in ("min_lines", "min_branches", "min_functions"):
        value = getattr(args, option)
        if not 0 <= value <= 100:
            parser.error(
                f"--{option.replace('_', '-')} must be between 0 and 100")

    try:
        totals = read_totals(args.lcov)
    except (OSError, ValueError) as error:
        print(f"{Path(argv[0]).name}: {error}", file=sys.stderr)
        return 2

    checks = (
        ("line", "LH", "LF", args.min_lines),
        ("branch", "BRH", "BRF", args.min_branches),
        ("function", "FNH", "FNF", args.min_functions),
    )

    failed = False
    for name, hit_key, total_key, minimum in checks:
        hit = totals[hit_key]
        total = totals[total_key]
        if total == 0:
            print(f"{name} coverage: no data")
            failed = True
            continue
        percentage = 100.0 * hit / total
        status = "ok" if percentage >= minimum else "FAIL"
        print(f"{name} coverage: {hit}/{total} = {percentage:.2f}% "
              f"(minimum {minimum:g}%) [{status}]")
        if percentage < minimum:
            failed = True

    if failed:
        print("coverage thresholds not met", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
