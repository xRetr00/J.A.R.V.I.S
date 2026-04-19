import json
import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


SEVERITIES = {"blocking", "warning", "info"}


@dataclass
class Finding:
    severity: str
    check: str
    message: str
    path: Optional[str] = None
    line: Optional[int] = None
    details: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "severity": self.severity,
            "check": self.check,
            "message": self.message,
            "path": self.path,
            "line": self.line,
            "details": self.details,
        }


def make_finding(
    check: str,
    message: str,
    severity: str = "warning",
    path: Optional[str] = None,
    line: Optional[int] = None,
    details: Optional[str] = None,
) -> Dict[str, Any]:
    normalized = severity if severity in SEVERITIES else "warning"
    return Finding(
        severity=normalized,
        check=check,
        message=message,
        path=path,
        line=line,
        details=details,
    ).to_dict()


def load_json(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_text(path: Path) -> str:
    with path.open("r", encoding="utf-8") as handle:
        return handle.read()


def to_abs(repo_root: Path, maybe_rel: str) -> Path:
    candidate = Path(maybe_rel)
    if candidate.is_absolute():
        return candidate
    return (repo_root / candidate).resolve()


def normalize_path(path: Path, repo_root: Path) -> str:
    try:
        rel = path.resolve().relative_to(repo_root.resolve())
        return rel.as_posix()
    except ValueError:
        return path.as_posix()


def run_command(command: str, cwd: Path, timeout_sec: int = 1200) -> Dict[str, Any]:
    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd),
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout_sec,
        )
        return {
            "command": command,
            "exit_code": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
    except subprocess.TimeoutExpired as exc:
        return {
            "command": command,
            "exit_code": 124,
            "stdout": exc.stdout or "",
            "stderr": (exc.stderr or "") + "\nCommand timed out.",
        }


def find_line_for_substring(text: str, needle: str) -> Optional[int]:
    index = text.find(needle)
    if index < 0:
        return None
    return text.count("\n", 0, index) + 1


def extract_function_body(source_text: str, qualified_name: str) -> Tuple[Optional[str], Optional[int], Optional[int]]:
    signature_index = source_text.find(qualified_name + "(")
    if signature_index < 0:
        return None, None, None

    brace_start = source_text.find("{", signature_index)
    if brace_start < 0:
        return None, None, None

    depth = 0
    for index in range(brace_start, len(source_text)):
        char = source_text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                body = source_text[brace_start + 1:index]
                start_line = source_text.count("\n", 0, brace_start) + 1
                end_line = source_text.count("\n", 0, index) + 1
                return body, start_line, end_line

    return None, None, None


def strip_line_comment(line: str) -> str:
    comment_index = line.find("//")
    if comment_index >= 0:
        return line[:comment_index]
    return line


def nonempty_code_lines(body: str) -> List[str]:
    lines: List[str] = []
    for raw_line in body.splitlines():
        line = strip_line_comment(raw_line).strip()
        if not line:
            continue
        if line in {"{", "}", "};"}:
            continue
        lines.append(line)
    return lines


def load_helper_config(script_dir: Path) -> Dict[str, Any]:
    config_path = script_dir / "helper_config.json"
    if not config_path.exists():
        return {}
    return load_json(config_path)


def get_git_changed_files(repo_root: Path) -> List[str]:
    result = run_command("git diff --name-only", cwd=repo_root, timeout_sec=30)
    if result["exit_code"] != 0:
        return []
    return [line.strip() for line in result["stdout"].splitlines() if line.strip()]


def summarize_output(text: str, max_lines: int = 25) -> str:
    lines = [line for line in text.splitlines() if line.strip()]
    if len(lines) <= max_lines:
        return "\n".join(lines)
    return "\n".join(lines[-max_lines:])


def classify_verdict(findings: List[Dict[str, Any]]) -> str:
    has_blocking = any(item.get("severity") == "blocking" for item in findings)
    if has_blocking:
        return "FAIL"
    has_warning = any(item.get("severity") == "warning" for item in findings)
    if has_warning:
        return "PASS WITH ISSUES"
    return "PASS"


def repo_root_from_manifest(manifest_path: Path, manifest: Dict[str, Any]) -> Path:
    if "repository_root" in manifest and manifest["repository_root"]:
        declared = Path(manifest["repository_root"])
        if not declared.is_absolute():
            return (manifest_path.parent / declared).resolve()
        return declared.resolve()

    current = manifest_path.resolve().parent
    while current != current.parent:
        if (current / ".git").exists():
            return current
        current = current.parent
    return manifest_path.resolve().parent
