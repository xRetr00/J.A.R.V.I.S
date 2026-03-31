from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from statistics import median
from typing import Any

from PyQt6.QtGui import QColor, QImage

from orb_analysis.render import FrameResult


@dataclass(frozen=True)
class FrameMetrics:
    brightness_mean: float
    brightness_median: float
    brightness_max: float
    center_mean: float
    center_max: float
    edge_mean: float
    center_to_edge_ratio: float
    outer_ring_slope: float
    clipping_high_pct: float
    clipping_low_pct: float
    visible_pixel_pct: float
    radial_profile: list[float]

    def to_dict(self) -> dict[str, Any]:
        return {
            "brightness_mean": self.brightness_mean,
            "brightness_median": self.brightness_median,
            "brightness_max": self.brightness_max,
            "center_mean": self.center_mean,
            "center_max": self.center_max,
            "edge_mean": self.edge_mean,
            "center_to_edge_ratio": self.center_to_edge_ratio,
            "outer_ring_slope": self.outer_ring_slope,
            "clipping_high_pct": self.clipping_high_pct,
            "clipping_low_pct": self.clipping_low_pct,
            "visible_pixel_pct": self.visible_pixel_pct,
            "radial_profile": self.radial_profile,
        }


def measure_frame(frame: FrameResult, center_radius_pct: float = 0.10, radial_bins: int = 32) -> dict[str, Any]:
    image = QImage(str(frame.image_path)).convertToFormat(QImage.Format.Format_RGBA8888)
    if image.isNull():
        raise RuntimeError(f"Unable to load rendered frame {frame.image_path}")
    metrics = measure_image(image, center_radius_pct=center_radius_pct, radial_bins=radial_bins)
    return {
        **frame.to_dict(),
        **metrics.to_dict(),
    }


def measure_image(image: QImage, center_radius_pct: float = 0.10, radial_bins: int = 32) -> FrameMetrics:
    width = image.width()
    height = image.height()
    total_pixels = max(1, width * height)
    center_x = (width - 1) * 0.5
    center_y = (height - 1) * 0.5
    center_radius = min(width, height) * center_radius_pct
    center_radius_sq = center_radius * center_radius
    max_radius = max(1.0, min(width, height) * 0.5)

    brightness_values: list[float] = []
    center_values: list[float] = []
    edge_values: list[float] = []

    radial_sum = [0.0] * radial_bins
    radial_count = [0] * radial_bins
    visible_pixels = 0
    high_clip_samples = 0
    low_clip_samples = 0

    raw = image.constBits().asstring(image.sizeInBytes())
    stride = image.bytesPerLine()
    for y in range(height):
        row_offset = y * stride
        dy = y - center_y
        for x in range(width):
            offset = row_offset + (x * 4)
            r = raw[offset]
            g = raw[offset + 1]
            b = raw[offset + 2]
            a = raw[offset + 3]
            if a == 0:
                continue

            visible_pixels += 1
            luminance = _luminance(r, g, b)
            brightness_values.append(luminance)

            dx = x - center_x
            distance_sq = dx * dx + dy * dy
            distance = math.sqrt(distance_sq)
            normalized_radius = min(distance / max_radius, 1.0)
            bin_index = min(int(normalized_radius * radial_bins), radial_bins - 1)
            radial_sum[bin_index] += luminance
            radial_count[bin_index] += 1

            if distance_sq <= center_radius_sq:
                center_values.append(luminance)
            if normalized_radius >= 0.8:
                edge_values.append(luminance)

            high_clip_samples += int(r >= 250) + int(g >= 250) + int(b >= 250)
            low_clip_samples += int(r <= 5) + int(g <= 5) + int(b <= 5)

    if not brightness_values:
        return FrameMetrics(
            brightness_mean=0.0,
            brightness_median=0.0,
            brightness_max=0.0,
            center_mean=0.0,
            center_max=0.0,
            edge_mean=0.0,
            center_to_edge_ratio=0.0,
            outer_ring_slope=0.0,
            clipping_high_pct=0.0,
            clipping_low_pct=0.0,
            visible_pixel_pct=0.0,
            radial_profile=[0.0] * radial_bins,
        )

    radial_profile = [
        (radial_sum[index] / radial_count[index]) if radial_count[index] else 0.0
        for index in range(radial_bins)
    ]
    center_mean = _safe_mean(center_values)
    edge_mean = _safe_mean(edge_values)
    outer_start = max(0, int(radial_bins * 0.75))
    outer_ring = radial_profile[outer_start:]
    outer_ring_slope = 0.0
    if len(outer_ring) >= 2:
        outer_ring_slope = (outer_ring[-1] - outer_ring[0]) / (len(outer_ring) - 1)

    rgb_samples = max(1, visible_pixels * 3)
    return FrameMetrics(
        brightness_mean=_safe_mean(brightness_values),
        brightness_median=float(median(brightness_values)),
        brightness_max=max(brightness_values),
        center_mean=center_mean,
        center_max=max(center_values) if center_values else 0.0,
        edge_mean=edge_mean,
        center_to_edge_ratio=center_mean / max(edge_mean, 1e-6),
        outer_ring_slope=outer_ring_slope,
        clipping_high_pct=(high_clip_samples / rgb_samples) * 100.0,
        clipping_low_pct=(low_clip_samples / rgb_samples) * 100.0,
        visible_pixel_pct=(visible_pixels / total_pixels) * 100.0,
        radial_profile=radial_profile,
    )


def create_center_bright_synthetic(size: int = 256) -> QImage:
    image = QImage(size, size, QImage.Format.Format_RGBA8888)
    image.fill(QColor(0, 0, 0, 0))
    center = (size - 1) * 0.5
    max_radius = max(1.0, size * 0.5)
    for y in range(size):
        for x in range(size):
            distance = math.hypot(x - center, y - center) / max_radius
            value = max(0.0, 1.0 - distance)
            color = int(round(value * 255.0))
            image.setPixelColor(x, y, QColor(color, color, color, 255))
    return image


def create_clipped_patch_synthetic(size: int = 256) -> QImage:
    image = QImage(size, size, QImage.Format.Format_RGBA8888)
    image.fill(QColor(0, 0, 0, 255))
    start = size // 4
    end = size - start
    for y in range(start, end):
        for x in range(start, end):
            image.setPixelColor(x, y, QColor(255, 255, 255, 255))
    return image


def create_vignette_synthetic(size: int = 256) -> QImage:
    image = QImage(size, size, QImage.Format.Format_RGBA8888)
    center = (size - 1) * 0.5
    max_radius = max(1.0, size * 0.5)
    for y in range(size):
        for x in range(size):
            distance = math.hypot(x - center, y - center) / max_radius
            value = max(0.0, 1.0 - (distance * distance))
            color = int(round(value * 255.0))
            image.setPixelColor(x, y, QColor(color, color, color, 255))
    return image


def _luminance(r: int, g: int, b: int) -> float:
    return (0.299 * r + 0.587 * g + 0.114 * b) / 255.0


def _safe_mean(values: list[float]) -> float:
    if not values:
        return 0.0
    return sum(values) / len(values)

