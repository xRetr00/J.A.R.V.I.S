from __future__ import annotations

import platform
import subprocess

from .common import failure, success

SPEC = {
    "name": "computer_open_app",
    "title": "Open App",
    "description": "Launch an installed app on Windows.",
    "args_schema": {"type": "object", "properties": {"target": {"type": "string"}}, "required": ["target"]},
    "risk_level": "ui_automation",
    "platforms": ["windows"],
    "supports_background": True,
    "tags": ["desktop", "apps"],
}


def run(_service, args, _context):
    if platform.system().lower() != "windows":
        return failure("Unsupported platform", "App launch currently targets Windows.")
    target = str(args.get("target") or "").strip()
    if not target:
        return failure("Open app failed", "A target is required.")
    escaped_target = target.replace("'", "''")
    script = "$ErrorActionPreference='Stop'; Start-Process -FilePath '{}' | Out-Null".format(escaped_target)
    try:
        subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        detail = (exc.stderr or exc.stdout or "").strip()
        if not detail:
            detail = f"Unable to launch {target}."
        return failure("Open app failed", detail)
    return success("App launch requested", f"Launched {target}.", target=target)
