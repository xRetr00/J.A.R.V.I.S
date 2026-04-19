import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from common import classify_verdict, load_json, repo_root_from_manifest
import check_controller_leaks
import check_duplicate_strings
import check_god_object_risk
import check_step_manifest
import check_step_tests
import check_wrapper_theater


def flatten_findings(check_groups: List[List[Dict[str, Any]]]) -> List[Dict[str, Any]]:
    items: List[Dict[str, Any]] = []
    for group in check_groups:
        items.extend(group)
    return items


def print_findings(title: str, findings: List[Dict[str, Any]], severity: str) -> None:
    filtered = [item for item in findings if item.get("severity") == severity]
    if not filtered:
        return

    print(title)
    for finding in filtered:
        location = ""
        if finding.get("path"):
            line = finding.get("line")
            location = f" ({finding['path']}{':' + str(line) if line else ''})"
        print(f"- [{finding['check']}] {finding['message']}{location}")
        if finding.get("details"):
            print("  " + finding["details"].replace("\n", "\n  "))


def run_all(manifest: Dict[str, Any], repo_root: Path, run_tests: bool) -> List[Dict[str, Any]]:
    groups: List[List[Dict[str, Any]]] = []

    groups.append(check_step_manifest.run(manifest, repo_root))
    groups.append(check_controller_leaks.run(manifest, repo_root))
    groups.append(check_duplicate_strings.run(manifest, repo_root))
    groups.append(check_wrapper_theater.run(manifest, repo_root))
    groups.append(check_god_object_risk.run(manifest, repo_root, SCRIPT_DIR))
    groups.append(check_step_tests.run(manifest, repo_root, execute=run_tests))

    return flatten_findings(groups)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run decomposition step verification checks")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    parser.add_argument("--repo-root", help="Optional override for repo root")
    parser.add_argument("--skip-tests", action="store_true", help="Skip required command execution")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON output")
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = Path(args.repo_root).resolve() if args.repo_root else repo_root_from_manifest(manifest_path, manifest)

    findings = run_all(manifest, repo_root, run_tests=not args.skip_tests)
    verdict = classify_verdict(findings)

    if args.json:
        print(json.dumps({"verdict": verdict, "findings": findings}, indent=2))
    else:
        print(f"Step: {manifest.get('step_name', 'Unknown Step')}")
        print(f"Manifest: {manifest_path.as_posix()}")
        print(f"Repository root: {repo_root.as_posix()}")
        print(f"Verdict: {verdict}")
        print()

        print_findings("Blocking issues:", findings, "blocking")
        print_findings("Warnings:", findings, "warning")
        print_findings("Info:", findings, "info")

        if verdict == "FAIL":
            print("\nRecommended next action: fix blocking issues and rerun this kit.")
        elif verdict == "PASS WITH ISSUES":
            print("\nRecommended next action: review warnings before continuing.")
        else:
            print("\nRecommended next action: safe to continue to the next planned step.")

    if verdict == "PASS":
        raise SystemExit(0)
    if verdict == "PASS WITH ISSUES":
        raise SystemExit(1)
    raise SystemExit(2)


if __name__ == "__main__":
    main()
