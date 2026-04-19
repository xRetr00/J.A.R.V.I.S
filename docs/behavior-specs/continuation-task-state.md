# Continuation and Task State Behavior Spec

This spec defines how Vaxil should continue user work across turns. It is a migration-target behavior contract, not a description of the current implementation. Current code has useful building blocks, but target behavior requires a richer task-state model than a single recent thread.

## Product Contract

Vaxil should treat user work as a set of recent/actionable action threads. A follow-up should attach to the correct thread when the referent is clear, ask with concise choices when multiple referents are plausible, and start a fresh task when the user clearly gives a new instruction.

Continuation is not a phrase list. Phrases such as "open it", "continue", "retry that", and "what did you find" are cues, but the decision must be based on task state, user intent, evidence, current desktop context, recency, and risk.

## Required Behavior

- Track multiple recent/actionable threads, not only the last task.
- Store enough thread state to support follow-ups: user goal, lifecycle state, summary, key artifacts, source links, tool evidence, confidence, last update time, freshness/expiry, next-step candidates, and private-context flags.
- Use context-based expiry. A thread becomes less eligible as it gets older, as unrelated tasks intervene, as live state changes, or as a newer task supersedes it.
- Let completed tasks receive natural follow-ups until they expire or are superseded.
- Keep failed or blocked tasks resumable and explainable. The user can ask why it failed, retry it, continue through another route, or inspect partial evidence.
- Keep canceled tasks auditable but not automatically resumable. After cancellation, "what were you doing?" should explain; "continue" should not restart unless the user explicitly says retry or resume.
- Survive sleep/wake and short restarts when enough safe thread state is available, while still respecting expiry and private-context rules.
- Prefer safe inspection before asking when a follow-up is plausible but incomplete.
- Ask with choices when multiple threads or artifacts are plausible. Do not guess silently.
- The latest explicit user instruction overrides inferred continuation.
- Use memory only for preferences and stable user context. Memory must not resurrect an old task as active continuation unless a future spec explicitly allows task-history recall.
- Suppress private/sensitive context by default. Ask before grounding on it or using it to act.

## Evidence Rules

- A thread can be eligible for continuation without being grounded enough to answer.
- Stored thread evidence can support explanations and summaries.
- Re-check live context before acting or answering when state may have changed, the action is risky, the evidence is stale, or the prior evidence was weak.
- If evidence is weak or empty, Vaxil should say it does not have enough evidence yet, then safely inspect or ask for permission.
- Tool success alone is not enough. Vaxil should not claim it saw a page, opened a target, read content, or completed a user goal unless evidence supports that claim.
- Follow-up answers should be grounded in stored or refreshed evidence and should avoid invented details.

## User Interaction Rules

- Successful task completion should produce a brief grounded result plus one useful optional next step when obvious.
- Progress should be visual first. Spoken progress is reserved for task start, blockers, important completions, or when the user asks for status.
- Follow-up listening should be task-sensitive: longer after wake-initiated, unfinished, or actionable tasks; shorter after ordinary chat.
- A side-effecting follow-up should use risk-based confirmation. Low-risk reversible actions can run when the referent is clear; risky, sensitive, destructive, or external actions require confirmation.
- Confirmation prompts must restate the exact action and referent before execution.
- If the user gives unrelated input while a thread is active, Vaxil should treat the new instruction as fresh unless the user explicitly links it to the prior task.

## Current-System Mapping

Current implementation concepts that map to this target:

- `ActionThread`: close to the target thread record, but currently represents a single current/recent artifact.
- `ActionThreadTracker`: owns current continuation state today; target behavior needs a multi-thread tracker or equivalent focused task-state owner.
- `AssistantBehaviorPolicy::shouldContinueActionThread(...)`: current continuation decision point; target behavior needs it to evaluate multiple candidates, ambiguity, evidence strength, and private-context state.
- `ActionSession`, `ResponseMode`, and `TrustDecision`: useful per-turn models for progress, confirmation, and response shape.
- `TurnRuntimePlan::continuesActionThread`, `evidenceState`, and `PromptTurnContext::activeTaskState`: useful prompt-facing outputs for selected continuation state.
- `ToolResultEvidencePolicy`: current weak-evidence assessment should become part of thread confidence and follow-up grounding.

This spec does not require new logic in `AssistantController.cpp`. Implementation should favor focused state/policy modules and minimal orchestration wiring.

## Scenario Transcripts

### Clear Follow-Up After One Completed Task

User: Search for the latest Qt release notes.

Vaxil: Found the current release notes and summarized the main changes. I can open the source or compare it with your installed Qt version.

User: Open it.

Expected behavior: Vaxil attaches "open it" to the release-notes artifact, confirms only if policy requires it, opens the specific source, and reports the outcome from evidence.

### Ambiguous Follow-Up After Multiple Tasks

Recent threads:

- Searched Qt release notes and found a source URL.
- Read `D:\Vaxil\PLAN.md`.

User: Open it.

Expected behavior: Vaxil asks which item to open, listing concise choices. It does not silently choose the latest if both are plausible.

### Running Task And Recent Completed Task

Recent threads:

- Running: analyzing logs.
- Completed: opened browser search results.

User: Continue.

Expected behavior: Vaxil prefers the running task if the user does not specify another referent. If the running task needs user input, Vaxil states what is needed. It does not reopen the completed browser result unless the user chooses it.

### Failed Task Retry

Vaxil: I could not read the file because permission was denied.

User: Retry that.

Expected behavior: Vaxil attaches to the failed file-read thread, explains the previous blocker if useful, safely retries or proposes another route, and keeps the failure evidence available.

### Canceled Task Audit

User: Never mind.

Vaxil: Okay, I will not run that action.

User: What were you doing?

Expected behavior: Vaxil explains the canceled task and why it stopped. It does not resume the action unless the user explicitly says retry or resume.

### Sleep/Wake Or Short Restart

Before sleep: Vaxil completes a search and stores key source links.

After wake: User says, What did you find?

Expected behavior: Vaxil may continue from the saved thread if it is still contextually fresh. If freshness is uncertain, it states that and re-checks before making current claims.

### Weak Or Empty Evidence

Tool result: browser automation reports success, but extracted text is empty.

User: What courses did you see?

Expected behavior: Vaxil says it does not have enough page evidence yet, then safely inspects the visible/browser context or asks for permission. It must not invent visible courses.

### Side-Effecting Follow-Up

Previous task: Vaxil found a downloadable installer.

User: Install it.

Expected behavior: Vaxil identifies the exact installer, assesses risk, asks for confirmation with the referent and action restated, and only proceeds after approval.

### Private Context

Current context: private/sensitive app or window.

User: Summarize that.

Expected behavior: Vaxil does not silently use private content. It asks for permission or requests pasted/shared content before grounding a summary.

### Fresh Request Should Not Attach

Previous task: read startup logs.

User: Search for the latest AMD news.

Expected behavior: Vaxil starts a fresh search thread. The old log-reading thread remains in history until expiry but is not used for the news request.

## Behavioral Acceptance Checks

- Given one fresh completed thread with one artifact, "open it" resolves to that artifact.
- Given multiple plausible threads, "open it" produces a disambiguation prompt with concise choices.
- Given one running thread and one completed thread, "continue" attaches to the running thread unless the user references the completed one.
- Given a failed thread, "retry that" attaches to the failed task and preserves previous failure evidence.
- Given a canceled thread, "what were you doing?" explains the canceled task without resuming it.
- Given a short restart or sleep/wake, fresh non-private thread state remains available if it has not expired.
- Given weak evidence, Vaxil does not claim grounded success and performs safe inspection or asks.
- Given a side-effecting continuation, Vaxil applies risk-based confirmation and restates action plus referent.
- Given private context, Vaxil suppresses silent grounding and asks before using it.
- Given a clear fresh request, Vaxil starts a new thread and does not attach to prior state.

## Migration Notes

- Replace single-thread assumptions with a focused multi-thread state owner before broadening continuation behavior.
- Keep phrase cues as signals, not as the source of truth.
- Persist only safe, useful thread state; private content should be omitted or stored as a redacted marker.
- Feed selected thread state and evidence confidence into prompt assembly explicitly.
- Add characterization tests before changing current routing behavior.
- Do not implement this by adding policy logic to `AssistantController.cpp`; use focused ownership and thin orchestration wiring.
