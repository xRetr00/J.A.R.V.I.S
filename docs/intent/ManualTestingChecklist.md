# Smart Intent Manual Testing Checklist

## Quick Setup
- Start with `VAXIL_INTENT_ADVISOR_MODE=heuristic`.
- Confirm `route_audit` logging is enabled and `route_final` entries are written.

## High-Value Checks
- Greeting only routes to `local_response`.
- Greeting/smalltalk + request does **not** short-circuit social.
- Deterministic with social prefix routes to deterministic execution.
- Informational phrases with command-like words (e.g. `open source`) route to conversation.
- Continuation with valid action thread routes to continuation path.
- Continuation without usable context clarifies or escalates backend (no blind continuation).
- Ambiguous mixed command/query prompts produce clarification or backend escalation with reason code.
- Fresh context-reference prompts produce context-aware task candidate/route.

## Inspect In `route_final`
- `intent_confidence.final`
- `ambiguity_score`
- `advisor_mode`
- `advisor_suggestion` and `advisor_evaluation`
- `execution_candidates` ordering and penalties
- `arbitrator_reason_codes`
- `final_executed_route`
- `overrides`

## Classification Guide
- Expected behavior:
  - decision matches prompt intent and has clear reason code.
- Suspicious behavior:
  - low ambiguity with clarification, or high ambiguity with direct execution.
  - backend escalation on clearly deterministic/local-safe prompts.
- Likely threshold issue:
  - repeated over-clarification across clear requests.
  - repeated backend escalations with high confidence local candidates.
- Likely routing bug:
  - missing reason codes for major decision branch.
  - candidate order contradicts final route with no override reason.
  - context-reference prompts ignored despite fresh context evidence.

