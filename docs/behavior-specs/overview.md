# Vaxil Behavior Specifications

These specifications define target behavior for Vaxil as an always-on desktop assistant. They are migration-target documents: they describe how the product should behave and where the current system needs to move, not a claim that the implementation already satisfies every rule.

The specs are behavior-first. They should guide orchestration, state, prompts, tools, UI feedback, and tests without turning into one-off fixes for a single site, command phrase, or tool.

## Shared Vocabulary

- Task: a user-visible unit of work, such as reading a file, searching current information, opening an app, summarizing visible content, setting a timer, or drafting text.
- Action thread: the durable task context Vaxil can continue across turns. It includes the user goal, lifecycle state, evidence, artifacts, confidence, and next possible actions.
- Artifact: an output or reference produced by a task, such as a file path, URL, search result, summary, draft, window/app target, or extracted text.
- Evidence: information Vaxil can use to justify a response or action. Evidence can come from tools, desktop context, files, logs, browser state, memory, or the user, but each source has a confidence level and freshness limit.
- Follow-up: a user turn that depends on prior context, such as "open it", "what did you find", "retry that", "continue", or "why did it fail".
- Continuation: the behavior of attaching a follow-up to an existing action thread instead of treating it as a fresh request.
- Confirmation: an explicit user approval step before a risky, sensitive, destructive, or ambiguous side effect.
- Private context: sensitive or private desktop/app/window state that must not be silently used to ground answers or actions.

## Global Behavior Principles

- The latest explicit user instruction wins over inferred thread state, memory, and prior assumptions.
- Vaxil should safely inspect available context first when intent is plausible but unclear, then ask if ambiguity remains.
- Continuation must be generic across task types. It must not depend on narrow phrase patches or special handling for one website, one tool, or one task category.
- Evidence and task state are separate. A thread may exist while still being too weakly grounded to answer confidently.
- Low-signal tool success is not user-goal success. Vaxil must distinguish "a tool ran" from "the task was achieved."
- Private context should be suppressed by default and require user permission before grounding or action.
- Memory may shape preferences and style, but active task state is authoritative for continuation.
- User-visible behavior should remain concise: visual progress first, short grounded completion summaries, and spoken updates only when useful.

## Spec Index

- [Continuation and Task State](continuation-task-state.md): how Vaxil tracks work, resolves follow-ups, handles failures/cancellations, persists context, and decides when to ask.
- [Tool Use and Evidence](tool-use-evidence.md): when Vaxil must inspect, verify, cite, retry, or avoid unsupported claims.
- [Prompt and Orchestration Behavior](prompt-orchestration-behavior.md): how Vaxil selects context, routes turns, assembles prompts, and keeps orchestration behavior coherent.
- [User Interaction and Responses](user-interaction-responses.md): how Vaxil speaks, stays silent, asks, confirms, reports progress, handles interruptions, and finishes tasks.
- [Safety and Permissions](safety-permissions.md): how Vaxil gates risky actions, private context, memory writes, destructive operations, and autonomy.

## Current-System Mapping

Current Vaxil already has several related concepts:

- `ActionThread` stores one recent task-like continuity artifact with task type, user goal, summary, artifacts, sources, state, success, validity, and expiry.
- `ActionThreadTracker` currently exposes a single current thread.
- `ActionSession`, `ResponseMode`, `ToolPlan`, `TrustDecision`, and `InputRouteDecision` describe per-turn action, response, routing, and permission shape.
- `TurnRuntimePlan` carries continuation, selected tools, selected memory, evidence state, prompt context, and active behavioral constraints.
- `ToolResultEvidencePolicy` identifies weak evidence for tool results.
- `ActionRiskPermissionService` and `ToolPermissionRegistry` model risk, confirmation, permissions, and user overrides.
- `PromptAdapter` and `AiRequestCoordinator` are current prompt/request assembly surfaces.
- `ResponseFinalizer`, `ExecutionNarrator`, and UI surface state shape the final user-visible response.

The behavior specs may reference these concepts as implementation mapping notes, but product behavior remains the source of truth.
