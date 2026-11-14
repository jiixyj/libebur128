#!/usr/bin/env python3

import sys
from pathlib import Path


def source_exclusions(path: Path) -> tuple[set[int], set[int]]:
    line_exclusions: set[int] = set()
    branch_exclusions: set[int] = set()
    in_line_exclusion = False
    in_branch_exclusion = False

    for line_number, text in enumerate(
            path.read_text(encoding="utf-8").splitlines(), 1):
        branch_start = "LCOV_EXCL_BR_START" in text
        branch_stop = "LCOV_EXCL_BR_STOP" in text
        line_start = "LCOV_EXCL_START" in text
        line_stop = "LCOV_EXCL_STOP" in text

        if (in_branch_exclusion or branch_start or branch_stop or
                "LCOV_EXCL_BR_LINE" in text or
                "STAILQ_INIT(" in text or
                "STAILQ_REMOVE_HEAD(" in text or
                "STAILQ_INSERT_TAIL(" in text or
                "} while (0)" in text):
            branch_exclusions.add(line_number)
        if (in_line_exclusion or line_start or line_stop or
                "LCOV_EXCL_LINE" in text):
            line_exclusions.add(line_number)

        if branch_start:
            in_branch_exclusion = True
        if branch_stop:
            in_branch_exclusion = False
        if line_start:
            in_line_exclusion = True
        if line_stop:
            in_line_exclusion = False

    return line_exclusions, branch_exclusions


def filter_block(block: list[str], line_exclusions: set[int],
                 branch_exclusions: set[int]) -> list[str]:
    filtered: list[str] = []
    line_total = 0
    line_hit = 0
    branch_total = 0
    branch_hit = 0

    for record in block:
        if record.startswith("DA:"):
            fields = record[3:].split(",")
            line_number = int(fields[0])
            if line_number in line_exclusions:
                continue
            line_total += 1
            if int(fields[1]) > 0:
                line_hit += 1
            filtered.append(record)
        elif record.startswith("BRDA:"):
            fields = record[5:].split(",")
            line_number = int(fields[0])
            if (line_number in line_exclusions or
                    line_number in branch_exclusions):
                continue
            branch_total += 1
            if fields[-1] not in ("-", "0"):
                branch_hit += 1
            filtered.append(record)
        elif record.startswith(("LF:", "LH:", "BRF:", "BRH:")):
            continue
        else:
            filtered.append(record)

    filtered.extend((
        f"BRF:{branch_total}",
        f"BRH:{branch_hit}",
        f"LF:{line_total}",
        f"LH:{line_hit}",
        "end_of_record",
    ))
    return filtered


def filter_lcov(text: str) -> str:
    output: list[str] = []
    block: list[str] = []
    line_exclusions: set[int] = set()
    branch_exclusions: set[int] = set()
    exclusions_cache: dict[Path, tuple[set[int], set[int]]] = {}

    for record in text.splitlines():
        if record.startswith("SF:"):
            if block:
                raise ValueError("encountered SF record before end_of_record")
            source = Path(record[3:])
            if source not in exclusions_cache:
                exclusions_cache[source] = source_exclusions(source)
            line_exclusions, branch_exclusions = exclusions_cache[source]
            block.append(record)
        elif record == "end_of_record":
            if not block:
                raise ValueError("encountered end_of_record without SF record")
            output.extend(
                filter_block(block, line_exclusions, branch_exclusions))
            block = []
        elif block:
            block.append(record)
        else:
            output.append(record)

    if block:
        raise ValueError("missing end_of_record")

    return "\n".join(output) + ("\n" if output else "")


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(f"usage: {Path(argv[0]).name} INPUT OUTPUT", file=sys.stderr)
        return 2

    try:
        filtered = filter_lcov(Path(argv[1]).read_text(encoding="utf-8"))
        Path(argv[2]).write_text(filtered, encoding="utf-8")
    except (OSError, ValueError) as error:
        print(f"{Path(argv[0]).name}: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
