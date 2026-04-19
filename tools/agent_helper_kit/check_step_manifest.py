import argparse
from pathlib import Path
from typing import Any, Dict, List

from common import (
    get_git_changed_files,
    load_json,
    make_finding,
    normalize_path,
    repo_root_from_manifest,
    to_abs,
)


CHECK_NAME = "step_manifest"


REQUIRED_TOP_KEYS = [
    "step_name",
    "controller_file",
    "owner_files",
    "required_commands",
]


def run(manifest: Dict[str, Any], repo_root: Path) -> List[Dict[str, Any]]:
    findings: List[Dict[str, Any]] = []

    for key in REQUIRED_TOP_KEYS:
        if key not in manifest:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"Manifest missing required key: {key}",
                    severity="blocking",
                )
            )

    controller_file = manifest.get("controller_file")
    if isinstance(controller_file, str):
        controller_path = to_abs(repo_root, controller_file)
        if not controller_path.exists():
            findings.append(
                make_finding(
                    CHECK_NAME,
                    "Controller file path does not exist",
                    severity="blocking",
                    path=normalize_path(controller_path, repo_root),
                )
            )

    owner_files = manifest.get("owner_files", [])
    if not isinstance(owner_files, list) or not owner_files:
        findings.append(
            make_finding(
                CHECK_NAME,
                "owner_files is empty or not a list",
                severity="blocking",
            )
        )
    else:
        for owner in owner_files:
            owner_path = to_abs(repo_root, str(owner))
            if not owner_path.exists():
                findings.append(
                    make_finding(
                        CHECK_NAME,
                        "Owner file path does not exist",
                        severity="blocking",
                        path=normalize_path(owner_path, repo_root),
                    )
                )

    for key in ["forbidden_controller_patterns", "forbidden_controller_strings", "forbidden_mutation_patterns"]:
        value = manifest.get(key, [])
        if value and not isinstance(value, list):
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"{key} should be a list",
                    severity="blocking",
                )
            )

    wrapper_checks = manifest.get("wrapper_checks", [])
    if wrapper_checks:
        if not isinstance(wrapper_checks, list):
            findings.append(
                make_finding(
                    CHECK_NAME,
                    "wrapper_checks should be a list",
                    severity="blocking",
                )
            )
        else:
            for index, item in enumerate(wrapper_checks):
                if not isinstance(item, dict):
                    findings.append(
                        make_finding(
                            CHECK_NAME,
                            f"wrapper_checks[{index}] should be an object",
                            severity="blocking",
                        )
                    )
                    continue
                if "function" not in item or "delegate_pattern" not in item:
                    findings.append(
                        make_finding(
                            CHECK_NAME,
                            f"wrapper_checks[{index}] missing function or delegate_pattern",
                            severity="blocking",
                        )
                    )

    commands = manifest.get("required_commands", [])
    if not isinstance(commands, list) or not commands:
        findings.append(
            make_finding(
                CHECK_NAME,
                "required_commands must be a non-empty list",
                severity="blocking",
            )
        )

    for key in ["expected_changed_files", "god_object_targets"]:
        value = manifest.get(key, [])
        if value and not isinstance(value, list):
            findings.append(
                make_finding(
                    CHECK_NAME,
                    f"{key} should be a list when provided",
                    severity="warning",
                )
            )

    expected_changed = manifest.get("expected_changed_files", [])
    if isinstance(expected_changed, list) and expected_changed:
        expected_set = {str(item).replace("\\", "/") for item in expected_changed}
        actual_changed = {item.replace("\\", "/") for item in get_git_changed_files(repo_root)}

        if actual_changed:
            unexpected = sorted(path for path in actual_changed if path not in expected_set)
            if unexpected:
                findings.append(
                    make_finding(
                        CHECK_NAME,
                        "Changed files include paths outside manifest expected_changed_files",
                        severity="warning",
                        details="unexpected=" + ", ".join(unexpected),
                    )
                )

            missing = sorted(path for path in expected_set if path not in actual_changed)
            if missing:
                findings.append(
                    make_finding(
                        CHECK_NAME,
                        "Some expected_changed_files are not currently in git diff",
                        severity="info",
                        details="missing=" + ", ".join(missing),
                    )
                )
        else:
            findings.append(
                make_finding(
                    CHECK_NAME,
                    "No git diff detected; expected_changed_files check skipped",
                    severity="info",
                )
            )

    if not findings:
        findings.append(
            make_finding(
                CHECK_NAME,
                "Manifest structure looks valid",
                severity="info",
            )
        )

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate a decomposition step manifest")
    parser.add_argument("--manifest", required=True, help="Path to step manifest JSON")
    args = parser.parse_args()

    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    repo_root = repo_root_from_manifest(manifest_path, manifest)
    findings = run(manifest, repo_root)

    for finding in findings:
        print(f"[{finding['severity'].upper()}] {finding['message']}")


if __name__ == "__main__":
    main()
