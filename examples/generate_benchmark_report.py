from pathlib import Path

from analysis.reference_benchmark import (
    compare_audio,
)

from analysis.benchmark_report import (
    generate_benchmark_report,
)


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)


reference_path = (
    PROJECT_ROOT
    / "audio"
    / "input"
    / "test.wav"
)

target_path = (
    PROJECT_ROOT
    / "audio"
    / "output"
    / "test_amp_preamp_raw.wav"
)


report_path = (
    PROJECT_ROOT
    / "docs"
    / "benchmark_report.md"
)


results = compare_audio(
    reference_path,
    target_path,
)


generated_path = (
    generate_benchmark_report(
        results,
        report_path,
        reference_name="Clean DI",
        target_name="Our Amp Sim",
    )
)


print()
print(
    "========================================"
)

print(
    "       BENCHMARK REPORT GENERATED"
)

print(
    "========================================"
)

print()

print(
    f"Report: {generated_path}"
)

print()