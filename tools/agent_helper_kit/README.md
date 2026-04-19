# Agent Helper Kit (V1)

## Purpose
This toolkit is an internal engineering verifier for decomposition steps in Vaxil (for example, AssistantController extraction steps).

It is built for AI coding agents and maintainers to run after each step and answer:
- Is ownership really moved?
- Is prompt/template authority duplicated?
- Is the controller still doing hidden business logic?
- Is the extracted module already drifting toward centralization?
- Did required build/test checks pass?

This is not a user-facing feature.

## Folder Layout
- tools/agent_helper_kit/run_step_check.py: single entry script, runs all checks and returns a verdict.
- tools/agent_helper_kit/check_step_manifest.py: validates manifest shape and file references.
- tools/agent_helper_kit/check_controller_leaks.py: detects forbidden symbols/logic still in AssistantController.
- tools/agent_helper_kit/check_duplicate_strings.py: detects duplicated template authority markers across legacy/new modules.
- tools/agent_helper_kit/check_wrapper_theater.py: flags wrappers that should delegate but still branch/shape prompts.
- tools/agent_helper_kit/check_god_object_risk.py: warns on scope-creep risk in extracted modules.
- tools/agent_helper_kit/check_step_tests.py: runs required step commands and records pass/fail.
- tools/agent_helper_kit/helper_config.json: thresholds and keyword heuristics.

## How Agents Should Use It
Run this after each decomposition step before moving to the next step:

```powershell
python tools/agent_helper_kit/run_step_check.py --manifest docs/assistant-controller-decomposition/manifests/step-02.json
```

For a quick architecture-only pass without running commands:

```powershell
python tools/agent_helper_kit/run_step_check.py --manifest docs/assistant-controller-decomposition/manifests/step-02.json --skip-tests
```

For machine-readable output:

```powershell
python tools/agent_helper_kit/run_step_check.py --manifest docs/assistant-controller-decomposition/manifests/step-02.json --json
```

## Step Manifest
Each step should define a JSON manifest (example: docs/assistant-controller-decomposition/manifests/step-02.json) with:
- step_name
- controller_file
- owner_files
- expected_symbols_moved
- forbidden_controller_patterns
- forbidden_controller_strings
- forbidden_mutation_patterns
- duplicate_string_checks
- wrapper_checks
- god_object_targets
- required_commands

Recommended process for a new step:
1. Copy an existing manifest.
2. Update ownership expectations and forbidden patterns for that step.
3. Add required build/test commands for that step.
4. Run run_step_check.py against the new manifest.

## Verdict Meaning
- PASS: no blocking issues and no warnings.
- PASS WITH ISSUES: no blockers, but warnings exist and should be reviewed.
- FAIL: blocking issue(s) exist. Do not continue to the next step.

Exit codes from run_step_check.py:
- 0 = PASS
- 1 = PASS WITH ISSUES
- 2 = FAIL

## Practical Notes
- Local-only: no network calls.
- No telemetry.
- Standard library only.
- Heuristic checks: useful and fast, but not a formal proof.

## Limitations (Important)
- Wrapper and duplication detection are heuristic.
- C++ parsing is lightweight (signature + brace matching), not a full AST parser.
- False positives/negatives are possible.
- You should still do a code review for final sign-off.

## Example (Step 02)
Manifest already included:
- docs/assistant-controller-decomposition/manifests/step-02.json

Run:

```powershell
python tools/agent_helper_kit/run_step_check.py --manifest docs/assistant-controller-decomposition/manifests/step-02.json
```
