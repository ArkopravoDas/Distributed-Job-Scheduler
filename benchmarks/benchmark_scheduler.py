from __future__ import annotations

import csv
import pathlib
import re
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"
RESULTS_PATH = ROOT / "benchmarks" / "results.csv"

SCENARIOS = [
    ("independent", 100),
    ("independent", 1000),
    ("chain", 100),
    ("diamond_batch", 100),
]
WORKER_COUNTS = [1, 2, 4, 8]

OUTPUT_PATTERN = re.compile(
    r"scenario=(?P<scenario>\S+)\s+"
    r"task_count=(?P<task_count>\d+)\s+"
    r"worker_count=(?P<worker_count>\d+)\s+"
    r"status=(?P<status>\S+)\s+"
    r"total_ms=(?P<total_ms>\d+)\s+"
    r"tasks_per_second=(?P<tasks_per_second>[0-9.]+)"
)


def scheduler_executable() -> pathlib.Path:
    if sys.platform.startswith("win"):
        return BUILD_DIR / "scheduler_app.exe"
    return BUILD_DIR / "scheduler_app"


def build_scheduler() -> None:
    subprocess.run(
        ["cmake", "-S", ".", "-B", str(BUILD_DIR), "-G", "MinGW Makefiles"],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(BUILD_DIR), "--target", "scheduler_app"],
        cwd=ROOT,
        check=True,
    )


def run_case(scenario: str, task_count: int, worker_count: int) -> dict[str, str]:
    executable = scheduler_executable()
    completed = subprocess.run(
        [
            str(executable),
            "--benchmark",
            scenario,
            str(task_count),
            str(worker_count),
        ],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )

    line = completed.stdout.strip()
    match = OUTPUT_PATTERN.fullmatch(line)
    if match is None:
        raise RuntimeError(f"unexpected benchmark output: {line!r}")

    result = match.groupdict()
    print(line)
    return result


def write_results(rows: list[dict[str, str]]) -> None:
    RESULTS_PATH.parent.mkdir(parents=True, exist_ok=True)

    with RESULTS_PATH.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(
            csv_file,
            fieldnames=[
                "scenario",
                "task_count",
                "worker_count",
                "status",
                "total_ms",
                "tasks_per_second",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    build_scheduler()

    rows: list[dict[str, str]] = []
    for scenario, task_count in SCENARIOS:
        for worker_count in WORKER_COUNTS:
            rows.append(run_case(scenario, task_count, worker_count))

    write_results(rows)
    print(f"saved results to {RESULTS_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
