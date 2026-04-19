import argparse
import re
from pathlib import Path
from typing import Any, Dict, List

from common import (
    extract_function_body,
    load_json,
    load_text,
    make_finding,
    nonempty_code_lines,
    normalize_path,
    repo_root_from_manifest,
    to_abs,
)


CHECK_NAME = "wrapper_theater"
BRANCH_RE = re.compile(r"\b(if|switch|for|while)\s*\(")


def run(manifest: Dict[str, Any], repo_root: Path) -> List[Dict[str, Any]]:
    findings: List[Dict[str, Any]] = []

    controller_path = to_abs(repo_root, manifest["controller_file"])
    controller_text = load_text(controller_path)
    controller_rel = normalize_path(controller_path, repo_root)

    wrapper_checks = manifest.get("wrapper_checks", [])
    if not isinstance(wrapper_checks, list) or not wrapper_checks:
        return [
            make_finding(
                CHECK_NAME,
                "No wrapper checks configured",
                severity="warning",
                path=controller_rel,
            )
        ]

    for item in wrapper_checks:
        if not isinstance(item, dict):
            continue

        function = item.get("function")
        delegate_pattern = item.get("delegate_pattern")
        severity = item.get("severity", "warning")
        allow_branching = bool(item.get("allow_branching", False))
        max_nonempty_lines = int(item.get("max_nonempty_lines", 8))
        forbid_prompt_literals = bool(item.get("forbid_prompt_literals", True))

        if not function or not delegate_pattern:
            continue

        qualified = f"AssistantController::{function}"
        body, start_line, _ = extract_function_body(controller_text, qualified)
        if body is None:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Wrapper function not found: {qualified}",
                    severity="blocking",
                    path=controller_rel,
                )
            )
            continue

        if delegate_pattern not in body:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Wrapper does not delegate using expected pattern: {qualified}",
                    severity=severity,
                    path=controller_rel,
                    line=start_line,
                )
            )

        code_lines = nonempty_code_lines(body)
        if len(code_lines) > max_nonempty_lines:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Wrapper looks thick ({len(code_lines)} code lines): {qualified}",
                    severity="warning",
                    path=controller_rel,
                    line=start_line,
                )
            )

        if not allow_branching and BRANCH_RE.search(body):
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Wrapper has branching logic but should be pure delegation: {qualified}",
                    severity=severity,
                    path=controller_rel,
                    line=start_line,
                )
            )

        if forbid_prompt_literals and ("QStringLiteral(" in body or ".arg(" in body):
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Wrapper appears to shape templates/prompts: {qualified}",
                    severity=severity,
                    path=controller_rel,
                    line=start_line,
                )
            )

    if not [f for f in findings if f["severity"] in {"blocking", "warning"}]:
        findings.append(
            make_finding(
                CHECK_NAME,
                "Configured wrappers look delegation-clean",
                severity="info",
                path=controller_rel,
            )
        )

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description="Check wrapper theater risk in controller")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = repo_root_from_manifest(manifest_path, manifest)

    for finding in run(manifest, repo_root):
        where = f" ({finding.get('path', '')}:{finding.get('line', '')})" if finding.get("path") else ""
        print(f"[{finding['severity'].upper()}] {finding['message']}{where}")


if __name__ == "__main__":
    main()
