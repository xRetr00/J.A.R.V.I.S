import argparse
from pathlib import Path
from typing import Any, Dict, List

from common import (
    load_json,
    make_finding,
    repo_root_from_manifest,
    run_command,
    summarize_output,
)


CHECK_NAME = "step_tests"


def run(manifest: Dict[str, Any], repo_root: Path, execute: bool = True) -> List[Dict[str, Any]]:
    findings: List[Dict[str, Any]] = []
    commands = manifest.get("required_commands", [])

    if not isinstance(commands, list) or not commands:
        return [
            make_finding(
                CHECK_NAME,
                "No required_commands configured",
                severity="warning",
            )
        ]

    if not execute:
        findings.append(
            make_finding(
                CHECK_NAME,
                "Test/build execution skipped by flag",
                severity="info",
            )
        )
        return findings

    for command in commands:
        result = run_command(command, repo_root)
        if result["exit_code"] != 0:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Required command failed with exit code {result['exit_code']}",
                    severity="blocking",
                    details=(
                        "command=" + command + "\n"
                        + summarize_output(result["stdout"] + "\n" + result["stderr"]) 
                    ),
                )
            )
        else:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    "Required command passed",
                    severity="info",
                    details="command=" + command,
                )
            )

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description="Run step-required test/build commands")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    parser.add_argument("--dry-run", action="store_true", help="Do not execute commands")
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = repo_root_from_manifest(manifest_path, manifest)

    for finding in run(manifest, repo_root, execute=not args.dry_run):
        extra = f"\n{finding['details']}" if finding.get("details") else ""
        print(f"[{finding['severity'].upper()}] {finding['message']}{extra}")


if __name__ == "__main__":
    main()
