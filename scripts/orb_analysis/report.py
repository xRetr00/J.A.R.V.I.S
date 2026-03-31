from __future__ import annotations

import csv
import json
import math
from collections import defaultdict
from pathlib import Path
from typing import Any

from PyQt6.QtCore import QRectF, Qt
from PyQt6.QtGui import QColor, QFont, QImage, QPainter


def write_metrics_csv(metrics_rows: list[dict[str, Any]], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "sweep",
        "label",
        "time",
        "time_scale",
        "zoom",
        "exposure",
        "image_path",
        "width",
        "height",
        "brightness_mean",
        "brightness_median",
        "brightness_max",
        "center_mean",
        "center_max",
        "edge_mean",
        "center_to_edge_ratio",
        "outer_ring_slope",
        "clipping_high_pct",
        "clipping_low_pct",
        "visible_pixel_pct",
    ]
    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in metrics_rows:
            writer.writerow({key: row.get(key) for key in fieldnames})


def write_metrics_json(metrics_rows: list[dict[str, Any]], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "frame_count": len(metrics_rows),
        "strongest_brightness_frame": _best(metrics_rows, "brightness_mean"),
        "strongest_center_frame": _best(metrics_rows, "center_mean"),
        "highest_clipping_frame": _best(metrics_rows, "clipping_high_pct"),
    }
    payload = {
        "summary": summary,
        "frames": metrics_rows,
    }
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def create_contact_sheet(
    metrics_rows: list[dict[str, Any]],
    sweep_name: str,
    output_path: Path,
    columns: int = 3,
    thumb_size: int = 256,
    caption_height: int = 72,
) -> None:
    sweep_rows = [row for row in metrics_rows if row["sweep"] == sweep_name]
    if not sweep_rows:
        return

    rows = math.ceil(len(sweep_rows) / columns)
    padding = 20
    tile_width = thumb_size
    tile_height = thumb_size + caption_height
    canvas_width = (columns * tile_width) + ((columns + 1) * padding)
    canvas_height = (rows * tile_height) + ((rows + 1) * padding) + 60

    image = QImage(canvas_width, canvas_height, QImage.Format.Format_RGBA8888)
    image.fill(QColor("#08111b"))

    painter = QPainter(image)
    painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, True)
    painter.setPen(QColor("#dceaf7"))
    title_font = QFont("Segoe UI", 18)
    title_font.setBold(True)
    painter.setFont(title_font)
    painter.drawText(QRectF(0, 0, canvas_width, 50), Qt.AlignmentFlag.AlignCenter, f"{sweep_name} comparison")

    text_font = QFont("Segoe UI", 10)
    painter.setFont(text_font)
    for index, row in enumerate(sweep_rows):
        col = index % columns
        row_index = index // columns
        x = padding + col * (tile_width + padding)
        y = 60 + padding + row_index * (tile_height + padding)
        _draw_tile(painter, row, x, y, tile_width, thumb_size, caption_height)

    painter.end()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(str(output_path))


def write_markdown_report(
    metrics_rows: list[dict[str, Any]],
    output_path: Path,
    shader_path: Path,
    size: int,
    sweep_time: float,
    comparisons_enabled: bool,
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    brightest = _best(metrics_rows, "brightness_mean")
    dimmest = _worst(metrics_rows, "brightness_mean")
    strongest_center = _best(metrics_rows, "center_mean")
    highest_clip = _best(metrics_rows, "clipping_high_pct")

    deltas = []
    for sweep_name, sweep_rows in _group_by_sweep(metrics_rows).items():
        if len(sweep_rows) < 2:
            continue
        brightness_range = max(row["brightness_mean"] for row in sweep_rows) - min(
            row["brightness_mean"] for row in sweep_rows
        )
        center_range = max(row["center_mean"] for row in sweep_rows) - min(
            row["center_mean"] for row in sweep_rows
        )
        clip_range = max(row["clipping_high_pct"] for row in sweep_rows) - min(
            row["clipping_high_pct"] for row in sweep_rows
        )
        deltas.append(
            f"- `{sweep_name}`: brightness Δ={brightness_range:.4f}, center Δ={center_range:.4f}, clipping Δ={clip_range:.2f}%"
        )

    comparison_lines = (
        [
            "- `time` comparison: [`comparisons/time_comparison.png`](../comparisons/time_comparison.png)",
            "- `timeScale` comparison: [`comparisons/timeScale_comparison.png`](../comparisons/timeScale_comparison.png)",
            "- `zoom` comparison: [`comparisons/zoom_comparison.png`](../comparisons/zoom_comparison.png)",
            "- `exposure` comparison: [`comparisons/exposure_comparison.png`](../comparisons/exposure_comparison.png)",
        ]
        if comparisons_enabled
        else ["- Comparison sheet generation was skipped for this run."]
    )

    report = f"""# Orb Shader Report

## Run Summary
- Shader: `{shader_path}`
- Output size: `{size}x{size}`
- Rendered frames: `{len(metrics_rows)}`
- Parameter sweep base time: `{sweep_time:g}`

## Key Frames
- Brightest frame: `{brightest['label']}` with mean brightness `{brightest['brightness_mean']:.4f}`
- Dimmest frame: `{dimmest['label']}` with mean brightness `{dimmest['brightness_mean']:.4f}`
- Strongest center frame: `{strongest_center['label']}` with center mean `{strongest_center['center_mean']:.4f}`
- Highest clipping frame: `{highest_clip['label']}` with high clipping `{highest_clip['clipping_high_pct']:.2f}%`

## Sweep Deltas
{chr(10).join(deltas) if deltas else '- No sweep deltas available.'}

## Comparison Sheets
{chr(10).join(comparison_lines)}

## Metrics Files
- CSV: [`metrics/metrics.csv`](../metrics/metrics.csv)
- JSON: [`metrics/metrics.json`](../metrics/metrics.json)
"""
    output_path.write_text(report, encoding="utf-8")


def _draw_tile(
    painter: QPainter,
    row: dict[str, Any],
    x: int,
    y: int,
    tile_width: int,
    thumb_size: int,
    caption_height: int,
) -> None:
    image = QImage(str(Path(row["image_path"])))
    thumb = image.scaled(
        thumb_size,
        thumb_size,
        Qt.AspectRatioMode.KeepAspectRatio,
        Qt.TransformationMode.SmoothTransformation,
    )
    painter.fillRect(x, y, tile_width, thumb_size + caption_height, QColor("#102130"))
    painter.drawImage(x, y, thumb)
    painter.setPen(QColor("#dceaf7"))
    caption = (
        f"{row['label']}\n"
        f"mean={row['brightness_mean']:.3f} center={row['center_mean']:.3f}\n"
        f"clip={row['clipping_high_pct']:.2f}% edge={row['edge_mean']:.3f}"
    )
    painter.drawText(
        QRectF(x + 8, y + thumb_size + 6, tile_width - 16, caption_height - 12),
        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop,
        caption,
    )


def _group_by_sweep(metrics_rows: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in metrics_rows:
        grouped[row["sweep"]].append(row)
    return grouped


def _best(metrics_rows: list[dict[str, Any]], key: str) -> dict[str, Any]:
    return max(metrics_rows, key=lambda row: row[key])


def _worst(metrics_rows: list[dict[str, Any]], key: str) -> dict[str, Any]:
    return min(metrics_rows, key=lambda row: row[key])
