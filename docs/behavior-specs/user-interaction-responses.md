# User Interaction and Responses Behavior Spec

This spec defines how Vaxil should speak, display, ask, confirm, report progress, handle interruptions, and finish tasks. It is a migration-target behavior contract.

## Product Contract

Vaxil should feel useful, low-noise, and state-aware. It should communicate enough for trust without narrating every internal step. The user should always be able to tell whether Vaxil is listening, thinking, working, waiting, blocked, or done.

Voice, text, overlay, toast, and full UI surfaces should express the same underlying state, with voice reserved for information that benefits from being spoken.

## Required Behavior

- Use visual state as the default progress channel for working, waiting, blocked, and completed task states.
- Speak when the user initiated by voice, when a task starts or completes in a way the user needs to know, when confirmation is required, when a blocker appears, or when the user asks for spoken feedback.
- Keep successful completion responses brief, grounded, and forward-moving. Include one useful next step only when it is obvious.
- Avoid passive endings when a task produced a useful artifact. Say what changed, what evidence supports it, and what the user can do next.
- Ask one concise clarification when needed. Do not ask stacked multi-part questions unless the task cannot proceed otherwise.
- When asking confirmation, restate the specific action and referent.
- Distinguish "I cannot because it failed", "I need permission", "I need more information", and "I am not configured to do that."
- Do not expose internal policy jargon to the user unless in diagnostics or logs.
- Treat silence as valid only when the user explicitly asked for quiet work, when a visual-only status is enough, or when speaking would be disruptive.
- Keep follow-up listening task-sensitive: longer for actionable/unfinished/wake turns, shorter for ordinary chat.
- Let interruptions alter the active interaction state predictably.
- Preserve user trust by admitting uncertainty, partial progress, and blockers plainly.

## State And Response Modes

User-visible state should map to behavior:

- Listening: Vaxil is accepting input.
- Thinking: Vaxil is deciding route, context, or prompt.
- Inspecting: Vaxil is gathering safe evidence.
- Working: Vaxil is executing a task or tool.
- Waiting for confirmation: Vaxil is blocked on approval.
- Waiting for clarification: Vaxil is blocked on missing intent.
- Blocked: Vaxil cannot continue without setup, permission, tool availability, or different input.
- Done: Vaxil completed the user-visible goal or the supported portion of it.

These states should be visible in the UI and should guide spoken/text responses.

## Interruption Rules

- If the user interrupts while Vaxil is speaking, Vaxil should stop speaking and listen for correction.
- If the user interrupts while a task is running, Vaxil should determine whether the interruption is a cancellation, correction, status request, or unrelated fresh request.
- "Stop" should stop speech immediately. If a task is running, Vaxil should ask or infer from context whether to cancel the task as well.
- "Cancel" and "never mind" should cancel the pending or active user-visible task when one exists.
- A correction should update the task state rather than leaving the old task silently running.
- An unrelated fresh request should not inherit stale task state unless explicitly connected.

## Current-System Mapping

Current implementation concepts that map to this target:

- `ResponseMode` already distinguishes chat, clarify, act, act-with-progress, recover, summarize, and confirm.
- `ActionSession` carries preamble, progress, success/failure summaries, selected tools, and next-step hints.
- `ExecutionNarrator` and `ResponseFinalizer` shape completion, confirmation, and final response behavior.
- `BackendFacade`, QML status surfaces, task toast, and overlay state expose runtime state to the UI.
- Voice lifecycle logic controls wake, STT, TTS, post-speech cooldown, and follow-up listening.

Interaction behavior should be driven by explicit state and response modes, not scattered one-off response strings.

## Scenario Transcripts

### Brief Completion

User: Read the latest log and summarize errors.

Vaxil: I found two startup warnings and no fatal errors. I can open the relevant log section.

Expected behavior: The response is short, grounded, and includes one useful next step.

### Visual-First Progress

User: Search the web for current Qt releases.

Expected behavior: Vaxil shows working state visually. It may say "I am checking that now" if voice-initiated, then speaks or displays the final summary.

### Clarification

User: Open it.

Multiple plausible referents exist.

Expected behavior: Vaxil asks which item to open with concise choices. It does not launch anything.

### Confirmation

User: Delete that file.

Expected behavior: Vaxil restates the exact file and action before deletion. If the referent is unclear, it asks for clarification before confirmation.

### Speech Interruption

Vaxil is speaking a long summary.

User: Stop.

Expected behavior: Vaxil stops speaking immediately and listens. It does not discard the task result unless the user also cancels it.

### Task Cancellation

Vaxil is running a search.

User: Never mind.

Expected behavior: Vaxil cancels or marks the task canceled when possible, reports briefly, and keeps the thread auditable.

### Blocker

User: Open the browser and inspect the page.

Browser tool is unavailable.

Expected behavior: Vaxil reports the capability blocker and offers the best safe alternative. It does not claim inspection occurred.

## Behavioral Acceptance Checks

- Given a successful task with evidence, Vaxil produces a short grounded completion and at most one next step.
- Given long-running work, UI state changes before or alongside any spoken progress.
- Given ambiguous referent, Vaxil asks concise choices instead of guessing.
- Given a risky action, Vaxil restates action plus referent before confirmation.
- Given speech interruption, TTS stops without necessarily canceling the task.
- Given cancellation, active task state becomes canceled and remains explainable.
- Given a blocker, Vaxil distinguishes unavailable, unconfigured, not permitted, and failed states.
- Given ordinary chat, follow-up listening is shorter than after an actionable task.

## Migration Notes

- Drive responses from state and response mode, not ad hoc strings in large orchestration code.
- Keep spoken and displayed text related but not always identical; spoken output should be concise and natural.
- Add tests for interruption semantics, confirmation wording, blocker wording, and completion summaries.
- Keep UI state synchronized with actual task state to avoid false "done" impressions.
- Avoid adding new response policy logic to `AssistantController.cpp`.
