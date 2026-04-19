import argparse
import re
from pathlib import Path
from typing import Any, Dict, List

from common import (
    find_line_for_substring,
    load_json,
    load_text,
    make_finding,
    normalize_path,
    repo_root_from_manifest,
    to_abs,
)


CHECK_NAME = "controller_leaks"


def _iter_entries(raw: List[Any], default_key: str) -> List[Dict[str, Any]]:
    entries: List[Dict[str, Any]] = []
    for item in raw:
        if isinstance(item, str):
            entries.append({default_key: item, "severity": "blocking"})
        elif isinstance(item, dict):
            entries.append(item)
    return entries


def run(manifest: Dict[str, Any], repo_root: Path) -> List[Dict[str, Any]]:
    findings: List[Dict[str, Any]] = []

    controller_path = to_abs(repo_root, manifest["controller_file"])
    controller_text = load_text(controller_path)
    controller_rel = normalize_path(controller_path, repo_root)

    pattern_entries = _iter_entries(manifest.get("forbidden_controller_patterns", []), "pattern")
    for entry in pattern_entries:
        pattern = entry.get("pattern")
        if not pattern:
            continue
        regex = re.compile(pattern, re.MULTILINE)
        match = regex.search(controller_text)
        if match:
            line = controller_text.count("\n", 0, match.start()) + 1
            findings.append(
                make_finding(
                    CHECK_NAME,
                    entry.get("message", f"Forbidden controller pattern matched: {pattern}"),
                    severity=entry.get("severity", "blocking"),
                    path=controller_rel,
                    line=line,
                )
            )

    string_entries = _iter_entries(manifest.get("forbidden_controller_strings", []), "text")
    for entry in string_entries:
        text = entry.get("text")
        if not text:
            continue
        if text in controller_text:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    entry.get("message", f"Forbidden string remains in controller: {text}"),
                    severity=entry.get("severity", "blocking"),
                    path=controller_rel,
                    line=find_line_for_substring(controller_text, text),
                )
            )

    mutation_entries = _iter_entries(manifest.get("forbidden_mutation_patterns", []), "pattern")
    for entry in mutation_entries:
        pattern = entry.get("pattern")
        if not pattern:
            continue
        regex = re.compile(pattern, re.MULTILINE)
        match = regex.search(controller_text)
        if match:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    entry.get("message", f"Forbidden mutation pattern in controller: {pattern}"),
                    severity=entry.get("severity", "blocking"),
                    path=controller_rel,
                    line=controller_text.count("\n", 0, match.start()) + 1,
                )
            )

    for symbol in manifest.get("expected_symbols_moved", []):
        if symbol and re.search(r"\b" + re.escape(symbol) + r"\b", controller_text):
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Symbol expected to be moved still appears in controller: {symbol}",
                    severity="blocking",
                    path=controller_rel,
                    line=find_line_for_substring(controller_text, symbol),
                )
            )

    if not findings:
        findings.append(
            make_finding(
                CHECK_NAME,
                "No obvious ownership leaks detected in controller",
                severity="info",
                path=controller_rel,
            )
        )

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description="Scan controller file for ownership leaks")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = repo_root_from_manifest(manifest_path, manifest)

    for finding in run(manifest, repo_root):
        where = ""
        if finding.get("path"):
            where = f" ({finding['path']}:{finding.get('line', '')})"
        print(f"[{finding['severity'].upper()}] {finding['message']}{where}")


if __name__ == "__main__":
    main()
