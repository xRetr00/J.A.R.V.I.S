import argparse
import re
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from common import (
    load_helper_config,
    load_json,
    load_text,
    make_finding,
    normalize_path,
    repo_root_from_manifest,
    to_abs,
)


CHECK_NAME = "god_object_risk"


def _extract_class_block(header_text: str, class_name: str) -> Optional[str]:
    marker = f"class {class_name}"
    class_index = header_text.find(marker)
    if class_index < 0:
        return None
    brace_start = header_text.find("{", class_index)
    if brace_start < 0:
        return None

    depth = 0
    for index in range(brace_start, len(header_text)):
        char = header_text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return header_text[brace_start + 1:index]
    return None


def _public_method_count(header_text: str, class_name: str) -> int:
    block = _extract_class_block(header_text, class_name)
    if block is None:
        return 0

    visibility = "private"
    count = 0
    for raw_line in block.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("//"):
            continue
        if line in {"public:", "private:", "protected:"}:
            visibility = line[:-1]
            continue
        if visibility != "public":
            continue
        if "(" in line and ")" in line and line.endswith(";"):
            count += 1
    return count


def _domain_hit_count(text: str, domain_keywords: Dict[str, List[str]]) -> int:
    lower = text.lower()
    hits = 0
    for _, keywords in domain_keywords.items():
        for keyword in keywords:
            if keyword.lower() in lower:
                hits += 1
                break
    return hits


def run(manifest: Dict[str, Any], repo_root: Path, script_dir: Path) -> List[Dict[str, Any]]:
    findings: List[Dict[str, Any]] = []
    targets = manifest.get("god_object_targets", [])
    if not isinstance(targets, list) or not targets:
        return [make_finding(CHECK_NAME, "No god-object targets configured", severity="info")]

    config = load_helper_config(script_dir)
    thresholds = config.get("god_object_thresholds", {})
    line_warning = int(thresholds.get("source_line_warning", 350))
    public_method_warning = int(thresholds.get("public_method_warning", 14))
    domain_warning = int(thresholds.get("domain_hit_warning", 4))
    suspicious_fragments = config.get("suspicious_name_fragments", [])
    domain_keywords = config.get("domain_keywords", {})

    for target in targets:
        if not isinstance(target, dict):
            continue

        class_name = target.get("class")
        header_path = to_abs(repo_root, target.get("header", ""))
        source_path = to_abs(repo_root, target.get("source", ""))

        if not header_path.exists() or not source_path.exists():
            findings.append(
                make_finding(
                    CHECK_NAME,
                    "God-object target file missing",
                    severity="warning",
                    details=f"header={header_path} source={source_path}",
                )
            )
            continue

        header_text = load_text(header_path)
        source_text = load_text(source_path)

        source_lines = len(source_text.splitlines())
        if source_lines >= line_warning:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Source file is getting large ({source_lines} lines)",
                    severity="warning",
                    path=normalize_path(source_path, repo_root),
                )
            )

        if class_name:
            method_count = _public_method_count(header_text, class_name)
            if method_count >= public_method_warning:
                findings.append(
                    make_finding(
                        CHECK_NAME,
                        f"Class has many public methods ({method_count}): {class_name}",
                        severity="warning",
                        path=normalize_path(header_path, repo_root),
                    )
                )

            for fragment in suspicious_fragments:
                if fragment.lower() in class_name.lower():
                    findings.append(
                        make_finding(
                            CHECK_NAME,
                            f"Class name contains risky broad fragment '{fragment}': {class_name}",
                            severity="warning",
                            path=normalize_path(header_path, repo_root),
                        )
                    )

        domain_hits = _domain_hit_count(header_text + "\n" + source_text, domain_keywords)
        if domain_hits >= domain_warning:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Module may mix multiple domains ({domain_hits} domain hits)",
                    severity="warning",
                    details=normalize_path(source_path, repo_root),
                )
            )

    if not [f for f in findings if f["severity"] == "warning"]:
        findings.append(make_finding(CHECK_NAME, "No major god-object risk flags", severity="info"))

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description="Risk-check extracted modules for god-object drift")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = repo_root_from_manifest(manifest_path, manifest)

    for finding in run(manifest, repo_root, script_dir):
        extra = f" | {finding['details']}" if finding.get("details") else ""
        print(f"[{finding['severity'].upper()}] {finding['message']}{extra}")


if __name__ == "__main__":
    main()
