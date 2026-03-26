#!/usr/bin/env python3
import argparse
import json
from dataclasses import asdict, dataclass


@dataclass
class Rect:
    x: float
    y: float
    width: float
    height: float

    @property
    def center_x(self) -> float:
        return self.x + self.width / 2.0

    @property
    def center_y(self) -> float:
        return self.y + self.height / 2.0


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def compute_layout(
    width: float,
    height: float,
    dpi_scale: float,
    presence_offset_x: float,
    presence_offset_y: float,
    has_results: bool,
):
    short_edge = min(width, height)
    page_margin = max(22.0 * dpi_scale, min(short_edge * 0.045, 56.0 * dpi_scale))
    section_spacing = max(12.0 * dpi_scale, short_edge * 0.012)
    content_width = min(width * 0.4, 520.0 * dpi_scale)
    orb_base_size = max(220.0 * dpi_scale, min(short_edge * 0.31, 420.0 * dpi_scale))
    side_lane_width = min(width * (0.3 if width >= 1480.0 * dpi_scale else 0.2), 470.0 * dpi_scale) if has_results else 0.0
    presence_drift_x = presence_offset_x * min(width * 0.04, 34.0 * dpi_scale)
    presence_drift_y = presence_offset_y * min(height * 0.03, 24.0 * dpi_scale)
    center_frame_width = max(content_width, orb_base_size)
    center_frame_height = orb_base_size + section_spacing + 88.0 * dpi_scale

    center_frame = Rect(
        x=(width - center_frame_width) / 2.0 + presence_drift_x,
        y=(height - center_frame_height) / 2.0 + height * 0.06 + presence_drift_y,
        width=center_frame_width,
        height=center_frame_height,
    )
    orb_rect = Rect(
        x=center_frame.center_x - orb_base_size / 2.0,
        y=center_frame.y + center_frame.height - orb_base_size,
        width=orb_base_size,
        height=orb_base_size,
    )
    task_panel = Rect(
        x=width - page_margin - side_lane_width,
        y=(height - min(height * 0.7, 640.0 * dpi_scale)) / 2.0,
        width=side_lane_width,
        height=min(height * 0.7, 640.0 * dpi_scale),
    )

    return {
        "inputs": {
            "width": width,
            "height": height,
            "dpiScale": dpi_scale,
            "presenceOffsetX": presence_offset_x,
            "presenceOffsetY": presence_offset_y,
            "hasResults": has_results,
        },
        "metrics": {
            "shortEdge": short_edge,
            "pageMargin": page_margin,
            "sectionSpacing": section_spacing,
            "contentWidth": content_width,
            "orbBaseSize": orb_base_size,
            "sideLaneWidth": side_lane_width,
            "presenceDriftX": presence_drift_x,
            "presenceDriftY": presence_drift_y,
        },
        "rects": {
            "centerFrame": asdict(center_frame),
            "orbRect": asdict(orb_rect),
            "taskPanel": asdict(task_panel),
        },
        "centers": {
            "screen": {"x": width / 2.0, "y": height / 2.0},
            "centerFrame": {"x": center_frame.center_x, "y": center_frame.center_y},
            "orb": {"x": orb_rect.center_x, "y": orb_rect.center_y},
        },
    }


def main():
    parser = argparse.ArgumentParser(description="Compute overlay and orb geometry from the QML formulas.")
    parser.add_argument("--width", type=float, required=True, help="Overlay window width")
    parser.add_argument("--height", type=float, required=True, help="Overlay window height")
    parser.add_argument("--dpi-scale", type=float, default=1.0, help="Effective dpi scale")
    parser.add_argument("--presence-offset-x", type=float, default=0.0, help="Normalized presence offset X (-1..1)")
    parser.add_argument("--presence-offset-y", type=float, default=0.0, help="Normalized presence offset Y (-1..1)")
    parser.add_argument("--has-results", action="store_true", help="Whether the right-side task lane is present")
    args = parser.parse_args()

    result = compute_layout(
        width=args.width,
        height=args.height,
        dpi_scale=max(args.dpi_scale, 1.0),
        presence_offset_x=clamp(args.presence_offset_x, -1.0, 1.0),
        presence_offset_y=clamp(args.presence_offset_y, -1.0, 1.0),
        has_results=args.has_results,
    )
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
