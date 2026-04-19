# Prompt and Orchestration Behavior Spec

This spec defines how Vaxil should route turns, select context, assemble prompts, and keep assistant behavior coherent across local responses, conversation, agent/tool work, continuations, and confirmations. It is a migration-target behavior contract.

## Product Contract

Vaxil should choose the narrowest behavior path that can satisfy the user goal with enough evidence and appropriate safety. Prompt construction should reflect the selected behavior path, not compensate for missing state management or unclear routing.

The prompt is not the only control plane. Task state, evidence state, permissions, route decisions, UI state, memory selection, and tool availability all participate in behavior.

## Required Behavior

- Route every turn through an explicit behavior decision: direct response, clarification, grounded answer, tool/action work, continuation, confirmation, recovery, or fresh task.
- Treat continuation as a state decision before prompt assembly. Do not rely on prompt wording alone to make a turn feel like a continuation.
- Select context by need: identity and current user message are baseline; memory, tools, desktop context, files, logs, vision, and prior evidence are included only when relevant and allowed.
- Prefer safe inspection before asking if the missing information can be derived without side effects or private-context use.
- Keep memory lane selection explicit. Preferences can shape tone; active task state and inspected evidence decide behavior.
- Include active task state and evidence confidence in prompts when a turn depends on prior work.
- Suppress private or sensitive context unless permission allows it.
- Keep prompts compact by default. Include large workspace/log/few-shot blocks only when the route requires them.
- Expose prompt assembly decisions in diagnostics: included blocks, suppressed blocks, selected tools, selected memory count, evidence state, and reason codes.
- Keep user-facing behavior independent from provider-specific quirks. Different AI backends should receive equivalent behavior contracts where possible.
- On backend/tool failure, route to recovery behavior instead of letting the prompt produce unsupported claims.

## Context Priority

When context sources disagree, Vaxil should apply this priority:

1. Latest explicit user instruction.
2. Current inspected evidence from tools, files, logs, browser, or desktop state.
3. Active action-thread state and artifacts.
4. Current session conversation history.
5. Stable user preferences and identity memory.
6. Older episodic memory.
7. Model prior knowledge.

This priority does not waive permission. Private or risky context still requires gating.

## Current-System Mapping

Current implementation concepts that map to this target:

- `InputRouter` and `AssistantBehaviorPolicy::decideRoute(...)` select high-level routes.
- `ActionSession`, `ResponseMode`, `ToolPlan`, and `TrustDecision` describe behavior mode, tool intent, progress, and confirmation.
- `TurnOrchestrationRuntime` builds a `TurnRuntimePlan` with continuation state, selected tools, selected memory, evidence state, prompt context, and prompt assembly report.
- `PromptAdapter` builds conversation, hybrid-agent, continuation, command, and agent-instruction prompts.
- `AiRequestCoordinator` coordinates request assembly/sending for conversation, agent, continuation, and command-extraction requests.
- `PromptAssemblyReport` already records included/suppressed blocks, selected tools, memory counts, evidence counts, and prompt character totals.

The target behavior should be implemented by focused policy and runtime modules with explicit inputs/outputs. `AssistantController.cpp` should remain orchestration wiring.

## Scenario Transcripts

### Simple Chat

User: Explain what a wake word is.

Expected behavior: Vaxil routes to direct conversation. It uses identity/preferences and normal conversation context, not file/log/browser tools.

### Freshness-Sensitive Question

User: What changed in the latest OpenAI API docs?

Expected behavior: Vaxil routes to grounded answer/tool use. Prompt includes evidence after inspection, not stale memory alone.

### Continuation Turn

Prior task: Vaxil searched release notes and stored sources.

User: Compare that with my installed version.

Expected behavior: Vaxil attaches to the release-notes thread, inspects local installed version if available, then compares from evidence.

### Private Context Present

User: What is on my screen?

Current desktop context is private.

Expected behavior: Vaxil does not include private context in the prompt. It asks for permission or asks the user to provide the content.

### Ambiguous Route

User: Check that for me.

Expected behavior: Vaxil first tries safe context resolution. If the referent is still unclear, it asks a concise clarification with likely options.

### Backend Failure

User: Summarize these logs.

AI backend fails after logs are read.

Expected behavior: Vaxil reports the backend failure and any safe local evidence available. It does not fabricate a full summary.

## Behavioral Acceptance Checks

- Given a simple stable question, Vaxil uses a compact conversation prompt without unnecessary tools.
- Given a freshness-sensitive query, Vaxil routes to grounded/tool behavior before final answer.
- Given a continuation cue with a clear thread, prompt context includes active task state and evidence confidence.
- Given private desktop context, prompt assembly suppresses private content.
- Given ambiguous context, Vaxil safely inspects first and asks only if ambiguity remains.
- Given conflicting memory and inspected evidence, prompt context favors inspected evidence.
- Given backend/tool failure, Vaxil routes to recovery behavior and avoids unsupported claims.
- Given prompt assembly diagnostics, included and suppressed blocks have useful reason codes.

## Migration Notes

- Keep prompt assembly explicit and auditable; avoid hidden context injection.
- Do not use prompt text as a substitute for structured continuation state.
- Keep provider-specific request details below the behavior contract.
- Add tests around route selection, prompt block inclusion, continuation context, and private-context suppression.
- Avoid moving broad routing policy into `AssistantController.cpp`.
