from __future__ import annotations

import argparse
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.append(str(Path(__file__).resolve().parent.parent))

from orb_analysis.metrics import measure_frame
from orb_analysis.render import OrbShaderRenderer, build_default_specs
from orb_analysis.report import (
    create_contact_sheet,
    write_markdown_report,
    write_metrics_csv,
    write_metrics_json,
)


DEFAULT_TIMES = [0.0, 0.5, 1.0, 2.0, 4.0, 8.0]
DEFAULT_TIME_SCALE_VALUES = [0.75, 1.0, 1.25]
DEFAULT_ZOOM_VALUES = [0.9, 1.0, 1.1]
DEFAULT_EXPOSURE_VALUES = [0.85, 1.0, 1.15]


def main() -> int:
    args = parse_args()
    shader_path = args.shader.resolve()
    output_root = args.out.resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    times = _parse_float_list(args.times, DEFAULT_TIMES)
    time_scale_values = _parse_float_list(args.time_scale_values, DEFAULT_TIME_SCALE_VALUES)
    zoom_values = _parse_float_list(args.zoom_values, DEFAULT_ZOOM_VALUES)
    exposure_values = _parse_float_list(args.exposure_values, DEFAULT_EXPOSURE_VALUES)

    specs = build_default_specs(
        times=times,
        time_scale_values=time_scale_values,
        zoom_values=zoom_values,
        exposure_values=exposure_values,
    )

    renderer = OrbShaderRenderer(shader_path=shader_path, output_root=output_root, size=args.size)
    try:
        frames = renderer.render_frames(specs)
    finally:
        renderer.close()

    metrics_rows = [measure_frame(frame) for frame in frames]

    metrics_dir = output_root / "metrics"
    write_metrics_csv(metrics_rows, metrics_dir / "metrics.csv")
    write_metrics_json(metrics_rows, metrics_dir / "metrics.json")

    if not args.skip_comparisons:
        comparisons_dir = output_root / "comparisons"
        for sweep_name in ("time", "timeScale", "zoom", "exposure"):
            create_contact_sheet(metrics_rows, sweep_name, comparisons_dir / f"{sweep_name}_comparison.png")

    if not args.skip_report:
        sweep_time = _select_base_time(times)
        report_dir = output_root / "report"
        write_markdown_report(
            metrics_rows=metrics_rows,
            output_path=report_dir / "orb_report.md",
            shader_path=shader_path,
            size=args.size,
            sweep_time=sweep_time,
            comparisons_enabled=not args.skip_comparisons,
        )

    print(f"Rendered {len(frames)} frames into {output_root}")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render and analyze orb.glsl without modifying source code.")
    parser.add_argument("--shader", type=Path, required=True, help="Path to the orb GLSL shader source.")
    parser.add_argument("--out", type=Path, required=True, help="Output directory for rendered frames and reports.")
    parser.add_argument("--size", type=int, default=1024, help="Square render resolution in pixels.")
    parser.add_argument("--times", type=str, default=None, help="Comma-separated time values.")
    parser.add_argument(
        "--time-scale-values",
        type=str,
        default=None,
        help="Comma-separated temporary wrapper timeScale values.",
    )
    parser.add_argument(
        "--zoom-values",
        type=str,
        default=None,
        help="Comma-separated temporary wrapper zoom values.",
    )
    parser.add_argument(
        "--exposure-values",
        type=str,
        default=None,
        help="Comma-separated temporary wrapper exposure values.",
    )
    parser.add_argument("--skip-report", action="store_true", help="Skip Markdown report generation.")
    parser.add_argument("--skip-comparisons", action="store_true", help="Skip comparison sheet generation.")
    return parser.parse_args()


def _parse_float_list(value: str | None, fallback: list[float]) -> list[float]:
    if value is None or not value.strip():
        return list(fallback)
    return [float(part.strip()) for part in value.split(",") if part.strip()]


def _select_base_time(times: list[float]) -> float:
    if 2.0 in times:
        return 2.0
    for candidate in times:
        if candidate > 0.0:
            return candidate
    return times[0]


if __name__ == "__main__":
    raise SystemExit(main())
