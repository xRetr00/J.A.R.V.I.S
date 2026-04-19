import argparse
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


CHECK_NAME = "duplicate_strings"


def _as_list(value: Any) -> List[Any]:
    return value if isinstance(value, list) else []


def run(manifest: Dict[str, Any], repo_root: Path) -> List[Dict[str, Any]]:
    findings: List[Dict[str, Any]] = []

    duplicate_checks = _as_list(manifest.get("duplicate_string_checks"))
    if not duplicate_checks:
        findings.append(
            make_finding(
                CHECK_NAME,
                "No duplicate string checks configured",
                severity="warning",
            )
        )
        return findings

    for item in duplicate_checks:
        if not isinstance(item, dict):
            continue

        marker = item.get("text", "")
        if not marker:
            continue

        legacy_files = [to_abs(repo_root, p) for p in _as_list(item.get("legacy_files"))]
        owner_files = [to_abs(repo_root, p) for p in _as_list(item.get("owner_files"))]
        severity = item.get("severity", "blocking")

        legacy_hits: List[str] = []
        owner_hits: List[str] = []

        for path in legacy_files:
            if not path.exists():
                continue
            text = load_text(path)
            if marker in text:
                legacy_hits.append(normalize_path(path, repo_root))

        for path in owner_files:
            if not path.exists():
                continue
            text = load_text(path)
            if marker in text:
                owner_hits.append(normalize_path(path, repo_root))

        if legacy_hits and owner_hits:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    item.get(
                        "message",
                        f"Marker appears in both legacy and owner files: {marker}",
                    ),
                    severity=severity,
                    details=(
                        "legacy=" + ", ".join(legacy_hits) + "; owner=" + ", ".join(owner_hits)
                    ),
                )
            )

        if not owner_hits:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Owner files missing expected marker: {marker}",
                    severity="warning",
                )
            )

    if not [f for f in findings if f["severity"] in {"blocking", "warning"}]:
        findings.append(
            make_finding(
                CHECK_NAME,
                "No duplicate marker drift detected",
                severity="info",
            )
        )

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description="Check duplicate template markers")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = repo_root_from_manifest(manifest_path, manifest)

    for finding in run(manifest, repo_root):
        extra = f" | {finding['details']}" if finding.get("details") else ""
        print(f"[{finding['severity'].upper()}] {finding['message']}{extra}")


if __name__ == "__main__":
    main()
