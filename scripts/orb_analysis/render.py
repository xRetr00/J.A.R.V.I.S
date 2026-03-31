from __future__ import annotations

import shutil
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

from PyQt6.QtCore import QCoreApplication, QUrl, Qt
from PyQt6.QtGui import QColor, QGuiApplication, QImage
from PyQt6.QtQuick import QQuickView, QQuickWindow, QSGRendererInterface


@dataclass(frozen=True)
class RenderSpec:
    sweep: str
    label: str
    time: float
    time_scale: float = 1.0
    zoom: float = 1.0
    exposure: float = 1.0

    def filename_stem(self) -> str:
        return (
            f"{self.sweep}"
            f"__t-{_slug_float(self.time)}"
            f"__timescale-{_slug_float(self.time_scale)}"
            f"__zoom-{_slug_float(self.zoom)}"
            f"__exposure-{_slug_float(self.exposure)}"
        )

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self)
        data["filename_stem"] = self.filename_stem()
        return data


@dataclass(frozen=True)
class FrameResult:
    spec: RenderSpec
    image_path: Path
    width: int
    height: int

    def to_dict(self) -> dict[str, Any]:
        return {
            **self.spec.to_dict(),
            "image_path": str(self.image_path),
            "width": self.width,
            "height": self.height,
        }


class OrbShaderRenderer:
    def __init__(self, shader_path: Path, output_root: Path, size: int) -> None:
        self.shader_path = shader_path.resolve()
        self.output_root = output_root.resolve()
        self.size = int(size)
        self.runtime_dir = self.output_root / "runtime"
        self.frames_dir = self.output_root / "frames"
        self.runtime_dir.mkdir(parents=True, exist_ok=True)
        self.frames_dir.mkdir(parents=True, exist_ok=True)

        self._app = _ensure_app()
        self._qsb_path = _resolve_qsb_path()
        self._shader_qsb = self._build_runtime_shader()
        self._qml_path = self._write_runtime_qml()
        self._view = self._create_view()

    def render_frames(self, specs: list[RenderSpec]) -> list[FrameResult]:
        results: list[FrameResult] = []
        for spec in specs:
            image = self.render_image(spec)
            image_path = self.frames_dir / f"{spec.filename_stem()}.png"
            if not image.save(str(image_path)):
                raise RuntimeError(f"Failed to save frame to {image_path}")
            results.append(FrameResult(spec=spec, image_path=image_path, width=image.width(), height=image.height()))
        return results

    def render_image(self, spec: RenderSpec) -> QImage:
        root = self._view.rootObject()
        if root is None:
            raise RuntimeError("QML root object is not available for rendering")
        root.setProperty("time", float(spec.time))
        root.setProperty("timeScale", float(spec.time_scale))
        root.setProperty("zoom", float(spec.zoom))
        root.setProperty("exposure", float(spec.exposure))
        self._pump_events(cycles=12)
        image = self._view.grabWindow()
        if image.isNull():
            raise RuntimeError(f"Qt returned an empty image for render spec {spec}")
        return image.convertToFormat(QImage.Format.Format_RGBA8888)

    def close(self) -> None:
        self._view.hide()
        self._pump_events(cycles=2)
        self._view.deleteLater()

    def _build_runtime_shader(self) -> Path:
        runtime_frag = self.runtime_dir / "orb_runtime_wrapper.frag"
        runtime_qsb = self.runtime_dir / "orb_runtime_wrapper.frag.qsb"
        source = self.shader_path.read_text(encoding="utf-8")
        source = _strip_version_directives(source)
        runtime_frag.write_text(_build_wrapper_shader(source), encoding="utf-8")
        subprocess.run(
            [str(self._qsb_path), "--qt6", "-o", str(runtime_qsb), str(runtime_frag)],
            check=True,
            capture_output=True,
            text=True,
        )
        return runtime_qsb

    def _write_runtime_qml(self) -> Path:
        qml_path = self.runtime_dir / "orb_runtime_view.qml"
        qml_path.write_text(
            _build_runtime_qml(self._shader_qsb.as_uri(), self.size),
            encoding="utf-8",
        )
        return qml_path

    def _create_view(self) -> QQuickView:
        view = QQuickView()
        view.setColor(QColor(0, 0, 0, 0))
        view.setFlags(Qt.WindowType.Tool | Qt.WindowType.FramelessWindowHint)
        view.setResizeMode(QQuickView.ResizeMode.SizeRootObjectToView)
        view.setX(-20000)
        view.setY(-20000)
        view.setWidth(self.size)
        view.setHeight(self.size)
        view.setSource(QUrl.fromLocalFile(str(self._qml_path)))
        if view.status() != QQuickView.Status.Ready:
            errors = "\n".join(error.toString() for error in view.errors())
            raise RuntimeError(f"Failed to load runtime QML:\n{errors}")
        view.show()
        self._pump_events(cycles=18)
        api = view.rendererInterface().graphicsApi()
        if api != QSGRendererInterface.GraphicsApi.OpenGL:
            raise RuntimeError(f"Qt Quick is using {api.name}, expected OpenGL for ShaderEffect rendering")
        return view

    def _pump_events(self, cycles: int) -> None:
        for _ in range(cycles):
            self._app.processEvents()
            QCoreApplication.sendPostedEvents(None, 0)


def build_default_specs(
    times: list[float],
    time_scale_values: list[float],
    zoom_values: list[float],
    exposure_values: list[float],
) -> list[RenderSpec]:
    specs: list[RenderSpec] = []
    for time_value in times:
        specs.append(RenderSpec(sweep="time", label=f"time={time_value:g}", time=float(time_value)))

    base_time = _select_base_time(times)
    for time_scale in time_scale_values:
        specs.append(
            RenderSpec(
                sweep="timeScale",
                label=f"timeScale={time_scale:g}",
                time=base_time,
                time_scale=float(time_scale),
            )
        )
    for zoom in zoom_values:
        specs.append(
            RenderSpec(
                sweep="zoom",
                label=f"zoom={zoom:g}",
                time=base_time,
                zoom=float(zoom),
            )
        )
    for exposure in exposure_values:
        specs.append(
            RenderSpec(
                sweep="exposure",
                label=f"exposure={exposure:g}",
                time=base_time,
                exposure=float(exposure),
            )
        )
    return specs


def _ensure_app() -> QGuiApplication:
    app = QGuiApplication.instance()
    if app is None:
        QQuickWindow.setGraphicsApi(QSGRendererInterface.GraphicsApi.OpenGL)
        app = QGuiApplication([])
    return app


def _resolve_qsb_path() -> Path:
    env_path = shutil.which("qsb")
    if env_path:
        return Path(env_path)
    default_path = Path(r"C:\Qt\6.10.2\msvc2022_64\bin\qsb.exe")
    if default_path.exists():
        return default_path
    raise FileNotFoundError("Unable to locate qsb.exe. Add it to PATH or install Qt shader tools.")


def _build_wrapper_shader(shader_source: str) -> str:
    return f"""#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {{
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2 resolution;
    float timeScale;
    float zoom;
    float exposure;
}} ubuf;

#define iTime (ubuf.time * ubuf.timeScale)
#define iResolution vec3(ubuf.resolution, 1.0)

{shader_source}

void main()
{{
    vec2 centeredUv = (qt_TexCoord0 - vec2(0.5)) * ubuf.zoom + vec2(0.5);
    vec2 fragCoord = centeredUv * ubuf.resolution;
    vec4 color = vec4(0.0);
    mainImage(color, fragCoord);
    color.rgb = max(color.rgb * ubuf.exposure, vec3(0.0));
    color.a = clamp(color.a, 0.0, 1.0);
    fragColor = color * ubuf.qt_Opacity;
}}
"""


def _build_runtime_qml(shader_uri: str, size: int) -> str:
    return f"""import QtQuick

Item {{
    width: {size}
    height: {size}

    property real time: 0.0
    property real timeScale: 1.0
    property real zoom: 1.0
    property real exposure: 1.0

    ShaderEffect {{
        anchors.fill: parent
        blending: true
        fragmentShader: "{shader_uri}"
        property real time: parent.time
        property vector2d resolution: Qt.vector2d(width, height)
        property real timeScale: parent.timeScale
        property real zoom: parent.zoom
        property real exposure: parent.exposure
    }}
}}
"""


def _strip_version_directives(source: str) -> str:
    lines = source.splitlines()
    return "\n".join(line for line in lines if not line.strip().startswith("#version")).strip() + "\n"


def _select_base_time(times: list[float]) -> float:
    if 2.0 in times:
        return 2.0
    for candidate in times:
        if candidate > 0.0:
            return float(candidate)
    return float(times[0]) if times else 0.0


def _slug_float(value: float) -> str:
    formatted = f"{value:.3f}".rstrip("0").rstrip(".")
    formatted = formatted.replace("-", "neg-")
    return formatted.replace(".", "p") if formatted else "0"
