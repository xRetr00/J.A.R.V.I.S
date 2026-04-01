#include "core/AssistantBehaviorPolicy.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonObject>
#include <QSet>

#include "core/InputRouter.h"

namespace {
QString normalizedText(const QString &input)
{
    return input.trimmed().toLower();
}

bool containsAny(const QString &input, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (input.contains(needle)) {
            return true;
        }
    }
    return false;
}

bool isProfileMemory(const MemoryRecord &record)
{
    const QString loweredType = record.type.toLower();
    const QString loweredKey = record.key.toLower();
    return loweredType == QStringLiteral("preference")
        || loweredKey == QStringLiteral("name")
        || loweredKey.contains(QStringLiteral("preference"))
        || loweredKey.contains(QStringLiteral("response_style"))
        || loweredKey.contains(QStringLiteral("voice"));
}

bool isActiveCommitmentMemory(const MemoryRecord &record)
{
    const QString loweredType = record.type.toLower();
    const QString loweredKey = record.key.toLower();
    const QString loweredValue = record.value.toLower();
    return loweredType == QStringLiteral("context")
        || loweredKey.contains(QStringLiteral("active"))
        || loweredKey.contains(QStringLiteral("current"))
        || loweredKey.contains(QStringLiteral("task"))
        || loweredKey.contains(QStringLiteral("project"))
        || loweredKey.contains(QStringLiteral("follow_up"))
        || loweredValue.contains(QStringLiteral("working on"))
        || loweredValue.contains(QStringLiteral("follow up"))
        || record.source.compare(QStringLiteral("runtime"), Qt::CaseInsensitive) == 0;
}

int riskScoreForTool(const QString &toolName)
{
    if (toolName == QStringLiteral("skill_install")) {
        return 95;
    }
    if (toolName == QStringLiteral("file_write")
        || toolName == QStringLiteral("file_patch")
        || toolName == QStringLiteral("computer_write_file")) {
        return 88;
    }
    if (toolName == QStringLiteral("computer_open_app")
        || toolName == QStringLiteral("computer_open_url")
        || toolName == QStringLiteral("browser_open")) {
        return 72;
    }
    if (toolName == QStringLiteral("memory_delete")) {
        return 70;
    }
    if (toolName == QStringLiteral("memory_write")
        || toolName == QStringLiteral("computer_set_timer")
        || toolName == QStringLiteral("skill_create")) {
        return 54;
    }
    return 12;
}

bool isSideEffectingTool(const QString &toolName)
{
    return riskScoreForTool(toolName) >= 50;
}

bool requiresGroundingTool(const QString &toolName)
{
    return toolName == QStringLiteral("web_search")
        || toolName == QStringLiteral("web_fetch")
        || toolName == QStringLiteral("file_read")
        || toolName == QStringLiteral("file_search")
        || toolName == QStringLiteral("dir_list")
        || toolName == QStringLiteral("log_tail")
        || toolName == QStringLiteral("log_search")
        || toolName == QStringLiteral("ai_log_read")
        || toolName == QStringLiteral("memory_search")
        || toolName == QStringLiteral("browser_fetch_text");
}

QString reasonForTool(const QString &toolName)
{
    if (toolName == QStringLiteral("web_search")) {
        return QStringLiteral("Ground current or uncertain facts before answering.");
    }
    if (toolName == QStringLiteral("browser_open")) {
        return QStringLiteral("Use browser automation before falling back to the system browser.");
    }
    if (toolName == QStringLiteral("computer_open_url")) {
        return QStringLiteral("Open the system browser only when direct browser automation is unavailable.");
    }
    if (toolName == QStringLiteral("file_read") || toolName == QStringLiteral("dir_list")) {
        return QStringLiteral("Inspect workspace state before summarizing or editing.");
    }
    if (toolName == QStringLiteral("file_write") || toolName == QStringLiteral("file_patch")) {
        return QStringLiteral("This changes files and should stay explicit and grounded.");
    }
    if (toolName == QStringLiteral("memory_search") || toolName == QStringLiteral("memory_write")) {
        return QStringLiteral("Use memory as structured assistant continuity, not just a fact store.");
    }
    return QStringLiteral("Tool selected by capability fit.");
}

int affordanceScoreForTool(const QString &input, IntentType intent, const AgentToolSpec &tool)
{
    const QString toolName = tool.name;
    const QString lowered = input.toLower();
    int score = 0;

    switch (intent) {
    case IntentType::LIST_FILES:
        if (toolName == QStringLiteral("dir_list")) score += 220;
        if (toolName == QStringLiteral("file_search")) score += 160;
        break;
    case IntentType::READ_FILE:
        if (toolName == QStringLiteral("file_read")) score += 220;
        if (toolName == QStringLiteral("log_tail") || toolName == QStringLiteral("log_search") || toolName == QStringLiteral("ai_log_read")) score += 170;
        if (toolName == QStringLiteral("dir_list")) score += 120;
        break;
    case IntentType::WRITE_FILE:
        if (toolName == QStringLiteral("file_write") || toolName == QStringLiteral("file_patch")) score += 220;
        if (toolName == QStringLiteral("computer_write_file")) score += 180;
        if (toolName == QStringLiteral("dir_list") || toolName == QStringLiteral("file_read")) score += 110;
        break;
    case IntentType::MEMORY_WRITE:
        if (toolName == QStringLiteral("memory_write")) score += 220;
        if (toolName == QStringLiteral("memory_search")) score += 150;
        break;
    case IntentType::GENERAL_CHAT:
    default:
        if (toolName == QStringLiteral("web_search")) score += 110;
        if (toolName == QStringLiteral("memory_search")) score += 100;
        if (toolName == QStringLiteral("file_read") || toolName == QStringLiteral("dir_list")) score += 92;
        break;
    }

    if (containsAny(lowered, {QStringLiteral("latest"), QStringLiteral("today"), QStringLiteral("current"), QStringLiteral("news")})) {
        if (toolName == QStringLiteral("web_search") || toolName == QStringLiteral("web_fetch")) {
            score += 150;
        }
    }

    if (containsAny(lowered, {QStringLiteral("browser"), QStringLiteral("website"), QStringLiteral("url"), QStringLiteral("open ")})) {
        if (toolName == QStringLiteral("browser_open")) {
            score += 150;
        }
        if (toolName == QStringLiteral("computer_open_url")) {
            score += 110;
        }
        if (toolName == QStringLiteral("browser_fetch_text")) {
            score += 100;
        }
    }

    if (containsAny(lowered, {QStringLiteral("app"), QStringLiteral("launch"), QStringLiteral("open app")})) {
        if (toolName == QStringLiteral("computer_open_app") || toolName == QStringLiteral("computer_list_apps")) {
            score += 130;
        }
    }

    if (containsAny(lowered, {QStringLiteral("read"), QStringLiteral("file"), QStringLiteral("log"), QStringLiteral("show"), QStringLiteral("inspect")})) {
        if (toolName == QStringLiteral("file_read") || toolName == QStringLiteral("file_search") || toolName == QStringLiteral("dir_list")) {
            score += 105;
        }
    }

    if (containsAny(lowered, {QStringLiteral("write"), QStringLiteral("create"), QStringLiteral("patch"), QStringLiteral("edit"), QStringLiteral("save")})) {
        if (toolName == QStringLiteral("file_write") || toolName == QStringLiteral("file_patch") || toolName == QStringLiteral("computer_write_file")) {
            score += 125;
        }
    }

    if (containsAny(lowered, {QStringLiteral("remember"), QStringLiteral("preference"), QStringLiteral("prefer"), QStringLiteral("forget")})) {
        if (toolName.startsWith(QStringLiteral("memory_"))) {
            score += 120;
        }
    }

    if (containsAny(lowered, {QStringLiteral("timer"), QStringLiteral("remind")})) {
        if (toolName == QStringLiteral("computer_set_timer")) {
            score += 135;
        }
    }

    if (requiresGroundingTool(toolName)) {
        score += 12;
    }

    if (toolName == QStringLiteral("browser_open") && lowered.contains(QStringLiteral("browser"))) {
        score += 40;
    }
    if (toolName == QStringLiteral("computer_open_url") && lowered.contains(QStringLiteral("browser"))) {
        score -= 20;
    }

    return score;
}

QString routeGoal(const InputRouteDecision &decision)
{
    switch (decision.kind) {
    case InputRouteKind::LocalResponse:
        return QStringLiteral("Respond directly");
    case InputRouteKind::BackgroundTasks:
    case InputRouteKind::DeterministicTasks:
        return QStringLiteral("Handle the requested task");
    case InputRouteKind::AgentConversation:
        return QStringLiteral("Work through the request with tools as needed");
    case InputRouteKind::CommandExtraction:
        return QStringLiteral("Resolve and execute the requested command");
    case InputRouteKind::Conversation:
        return QStringLiteral("Answer the user naturally");
    case InputRouteKind::AgentCapabilityError:
        return QStringLiteral("Recover from unavailable agent capability");
    case InputRouteKind::None:
    default:
        return QStringLiteral("Handle the request");
    }
}

QString sessionIdFor(const QString &input, const InputRouteDecision &decision)
{
    const QByteArray digest = QCryptographicHash::hash(
        QStringLiteral("%1|%2|%3")
            .arg(QString::number(static_cast<int>(decision.kind)),
                 QString::number(QDateTime::currentMSecsSinceEpoch()),
                 input.left(160))
            .toUtf8(),
        QCryptographicHash::Sha1);
    return QString::fromLatin1(digest.toHex().left(12));
}

bool containsActionRestatement(const QString &input)
{
    return containsAny(input, {
        QStringLiteral("open"),
        QStringLiteral("launch"),
        QStringLiteral("write"),
        QStringLiteral("create"),
        QStringLiteral("save"),
        QStringLiteral("install"),
        QStringLiteral("delete"),
        QStringLiteral("remove"),
        QStringLiteral("set"),
        QStringLiteral("remember"),
        QStringLiteral("do it"),
        QStringLiteral("go ahead"),
        QStringLiteral("proceed")
    });
}
}

InputRouteDecision AssistantBehaviorPolicy::decideRoute(const InputRouterContext &context) const
{
    InputRouteDecision decision;

    if (context.wakeOnly) {
        decision.kind = InputRouteKind::LocalResponse;
        decision.status = QStringLiteral("Listening");
        decision.speak = false;
        return decision;
    }

    if (context.shouldEndConversation) {
        decision.kind = InputRouteKind::LocalResponse;
        decision.status = QStringLiteral("Conversation ended");
        return decision;
    }

    if (context.isTimeQuery || context.isDateQuery) {
        decision.kind = InputRouteKind::LocalResponse;
        decision.status = context.isTimeQuery
            ? QStringLiteral("Local time response")
            : QStringLiteral("Local date response");
        return decision;
    }

    if (context.hasDeterministicTask) {
        decision.kind = InputRouteKind::DeterministicTasks;
        decision.tasks = {context.deterministicTask};
        decision.message = context.deterministicSpoken;
        decision.status = QStringLiteral("Background task queued");
        return decision;
    }

    if (context.agentEnabled && context.explicitComputerControl) {
        decision.kind = InputRouteKind::AgentConversation;
        decision.intent = IntentType::GENERAL_CHAT;
        return decision;
    }

    if (context.backgroundIntentReady && !context.backgroundTasks.isEmpty()) {
        decision.kind = InputRouteKind::BackgroundTasks;
        decision.tasks = context.backgroundTasks;
        decision.message = context.backgroundSpokenMessage;
        return decision;
    }

    if (context.localIntent == LocalIntent::Greeting || context.localIntent == LocalIntent::SmallTalk) {
        decision.kind = InputRouteKind::LocalResponse;
        decision.status = QStringLiteral("Local response");
        return decision;
    }

    if (context.visionRelevant && !context.explicitComputerControl && !context.directVisionResponse.trimmed().isEmpty()) {
        decision.kind = InputRouteKind::LocalResponse;
        decision.message = context.directVisionResponse;
        decision.status = QStringLiteral("Vision response");
        return decision;
    }

    if (context.explicitToolInventory) {
        decision.kind = InputRouteKind::LocalResponse;
        decision.message = context.toolInventoryText;
        decision.status = QStringLiteral("Tool inventory");
        return decision;
    }

    if (!context.aiAvailable) {
        decision.kind = InputRouteKind::AgentCapabilityError;
        decision.status = QStringLiteral("AI unavailable");
        return decision;
    }

    if (!context.visionRelevant && context.explicitWebSearch) {
        decision.kind = InputRouteKind::BackgroundTasks;
        AgentTask task;
        task.type = QStringLiteral("web_search");
        task.args = QJsonObject{{QStringLiteral("query"), context.explicitWebQuery}};
        task.priority = 85;
        decision.tasks = {task};
        return decision;
    }

    if (!context.visionRelevant && context.agentEnabled && context.likelyKnowledgeLookup) {
        decision.kind = InputRouteKind::BackgroundTasks;
        AgentTask task;
        task.type = QStringLiteral("web_search");
        task.args = QJsonObject{{QStringLiteral("query"), context.explicitWebQuery}};
        task.priority = 83;
        decision.tasks = {task};
        return decision;
    }

    if (!context.visionRelevant && context.agentEnabled && context.freshnessSensitive) {
        decision.kind = InputRouteKind::BackgroundTasks;
        AgentTask task;
        task.type = QStringLiteral("web_search");
        task.args = QJsonObject{
            {QStringLiteral("query"), context.explicitWebQuery},
            {QStringLiteral("freshness"), context.freshnessCode},
            {QStringLiteral("prefer_fresh"), true}
        };
        task.priority = 84;
        decision.tasks = {task};
        return decision;
    }

    if (context.agentEnabled && context.explicitAgentWorldQuery) {
        decision.kind = InputRouteKind::AgentConversation;
        decision.intent = context.effectiveIntent;
        return decision;
    }

    if (context.visionRelevant && !context.explicitComputerControl) {
        decision.kind = InputRouteKind::Conversation;
        decision.useVisionContext = true;
        return decision;
    }

    if (context.localIntent == LocalIntent::Command || context.likelyCommand) {
        decision.kind = InputRouteKind::CommandExtraction;
        return decision;
    }

    if (context.effectiveIntent != IntentType::GENERAL_CHAT
        && context.effectiveIntentConfidence > 0.4f
        && context.effectiveIntentConfidence <= 0.8f
        && context.agentEnabled) {
        decision.kind = InputRouteKind::AgentConversation;
        decision.intent = context.effectiveIntent;
        return decision;
    }

    decision.kind = InputRouteKind::Conversation;
    decision.useVisionContext = context.visionRelevant;
    decision.intent = context.effectiveIntent;
    return decision;
}

MemoryContext AssistantBehaviorPolicy::buildMemoryContext(const QString &input, const QList<MemoryRecord> &memory) const
{
    MemoryContext context;
    for (const MemoryRecord &record : memory) {
        if (isActiveCommitmentMemory(record)) {
            context.activeCommitments.push_back(record);
            continue;
        }
        if (isProfileMemory(record)) {
            context.profile.push_back(record);
            continue;
        }
        context.episodic.push_back(record);
    }

    auto trimLane = [](QList<MemoryRecord> &lane, int maxCount) {
        if (lane.size() > maxCount) {
            lane = lane.mid(0, maxCount);
        }
    };

    const QString loweredInput = normalizedText(input);
    auto sortLane = [&loweredInput](QList<MemoryRecord> &lane) {
        std::stable_sort(lane.begin(), lane.end(), [&loweredInput](const MemoryRecord &left, const MemoryRecord &right) {
            const QString leftText = (left.key + QLatin1Char(' ') + left.value).toLower();
            const QString rightText = (right.key + QLatin1Char(' ') + right.value).toLower();
            const bool leftRelevant = !loweredInput.isEmpty() && leftText.contains(loweredInput);
            const bool rightRelevant = !loweredInput.isEmpty() && rightText.contains(loweredInput);
            if (leftRelevant != rightRelevant) {
                return leftRelevant;
            }
            return left.updatedAt > right.updatedAt;
        });
    };

    sortLane(context.activeCommitments);
    sortLane(context.profile);
    sortLane(context.episodic);

    trimLane(context.activeCommitments, 4);
    trimLane(context.profile, 4);
    trimLane(context.episodic, 4);
    return context;
}

ToolPlan AssistantBehaviorPolicy::buildToolPlan(const QString &input,
                                                IntentType intent,
                                                const QList<AgentToolSpec> &availableTools) const
{
    ToolPlan plan;
    plan.goal = input.trimmed();

    struct RankedTool {
        AgentToolSpec spec;
        int affordanceScore = 0;
        int riskScore = 0;
        bool requiresGrounding = false;
        bool sideEffecting = false;
        QString reason;
    };

    QList<RankedTool> ranked;
    for (const AgentToolSpec &tool : availableTools) {
        const int affordance = affordanceScoreForTool(input, intent, tool);
        if (affordance <= 0) {
            continue;
        }

        RankedTool entry;
        entry.spec = tool;
        entry.affordanceScore = affordance;
        entry.riskScore = riskScoreForTool(tool.name);
        entry.requiresGrounding = requiresGroundingTool(tool.name);
        entry.sideEffecting = isSideEffectingTool(tool.name);
        entry.reason = reasonForTool(tool.name);
        ranked.push_back(entry);
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedTool &left, const RankedTool &right) {
        if (left.affordanceScore != right.affordanceScore) {
            return left.affordanceScore > right.affordanceScore;
        }
        if (left.requiresGrounding != right.requiresGrounding) {
            return left.requiresGrounding && !right.requiresGrounding;
        }
        if (left.riskScore != right.riskScore) {
            return left.riskScore < right.riskScore;
        }
        return left.spec.name < right.spec.name;
    });

    QSet<QString> seen;
    for (const RankedTool &entry : ranked) {
        if (seen.contains(entry.spec.name)) {
            continue;
        }
        seen.insert(entry.spec.name);
        plan.candidates.push_back({
            .toolName = entry.spec.name,
            .affordanceScore = entry.affordanceScore,
            .riskScore = entry.riskScore,
            .requiresGrounding = entry.requiresGrounding,
            .sideEffecting = entry.sideEffecting,
            .reason = entry.reason
        });
        plan.orderedToolNames.push_back(entry.spec.name);
        plan.requiresGrounding = plan.requiresGrounding || entry.requiresGrounding;
        plan.sideEffecting = plan.sideEffecting || entry.sideEffecting;
        if (plan.orderedToolNames.size() >= 10) {
            break;
        }
    }

    if (plan.orderedToolNames.isEmpty()) {
        for (const AgentToolSpec &tool : availableTools) {
            if (tool.name == QStringLiteral("web_search")
                || tool.name == QStringLiteral("memory_search")
                || tool.name == QStringLiteral("dir_list")
                || tool.name == QStringLiteral("file_read")) {
                plan.orderedToolNames.push_back(tool.name);
            }
        }
    }

    if (plan.requiresGrounding && plan.sideEffecting) {
        plan.rationale = QStringLiteral("Inspect and verify state before taking any side-effecting action.");
    } else if (plan.requiresGrounding) {
        plan.rationale = QStringLiteral("Ground the answer in retrieved evidence before responding.");
    } else if (plan.sideEffecting) {
        plan.rationale = QStringLiteral("The request changes state, so keep execution explicit and intentional.");
    } else {
        plan.rationale = QStringLiteral("Prefer the smallest useful tool surface for this request.");
    }

    return plan;
}

QList<AgentToolSpec> AssistantBehaviorPolicy::selectRelevantTools(const QString &input,
                                                                  IntentType intent,
                                                                  const QList<AgentToolSpec> &availableTools) const
{
    const ToolPlan plan = buildToolPlan(input, intent, availableTools);
    if (plan.orderedToolNames.isEmpty()) {
        return availableTools;
    }

    QList<AgentToolSpec> selected;
    QSet<QString> selectedNames;
    for (const QString &toolName : plan.orderedToolNames) {
        for (const AgentToolSpec &tool : availableTools) {
            if (tool.name == toolName && !selectedNames.contains(tool.name)) {
                selected.push_back(tool);
                selectedNames.insert(tool.name);
            }
        }
    }

    return selected.isEmpty() ? availableTools : selected;
}

TrustDecision AssistantBehaviorPolicy::assessTrust(const QString &input,
                                                   const InputRouteDecision &decision,
                                                   const ToolPlan &plan) const
{
    const QString lowered = normalizedText(input);
    const bool explicitActionVerb = containsAny(lowered,
        {QStringLiteral("open"),
         QStringLiteral("launch"),
         QStringLiteral("write"),
         QStringLiteral("create"),
         QStringLiteral("save"),
         QStringLiteral("install"),
         QStringLiteral("delete"),
         QStringLiteral("remove"),
         QStringLiteral("set a timer"),
         QStringLiteral("remember")});

    TrustDecision trust;
    trust.highRisk = plan.sideEffecting;
    trust.requiresConfirmation = plan.sideEffecting
        && !explicitActionVerb
        && (decision.kind == InputRouteKind::AgentConversation
            || decision.kind == InputRouteKind::BackgroundTasks
            || decision.kind == InputRouteKind::DeterministicTasks);

    if (trust.requiresConfirmation) {
        trust.reason = QStringLiteral("The request can change files, apps, memory, or system state.");
        trust.userMessage = QStringLiteral("This affects your system or stored state, so it should stay explicit.");
    } else if (trust.highRisk) {
        trust.reason = QStringLiteral("The request changes local state and should be narrated clearly.");
    } else if (plan.requiresGrounding) {
        trust.reason = QStringLiteral("The request needs grounded evidence before answering.");
    }

    return trust;
}

ResponseMode AssistantBehaviorPolicy::chooseResponseMode(const QString &input,
                                                         const InputRouteDecision &decision,
                                                         const TrustDecision &trust) const
{
    Q_UNUSED(input)

    if (trust.requiresConfirmation) {
        return ResponseMode::Confirm;
    }

    switch (decision.kind) {
    case InputRouteKind::BackgroundTasks:
    case InputRouteKind::DeterministicTasks:
        return ResponseMode::ActWithProgress;
    case InputRouteKind::AgentConversation:
    case InputRouteKind::CommandExtraction:
        return ResponseMode::Act;
    case InputRouteKind::AgentCapabilityError:
        return ResponseMode::Recover;
    case InputRouteKind::LocalResponse:
        return ResponseMode::Summarize;
    case InputRouteKind::Conversation:
        return ResponseMode::Chat;
    case InputRouteKind::None:
    default:
        return ResponseMode::Clarify;
    }
}

ActionSession AssistantBehaviorPolicy::createActionSession(const QString &input,
                                                           const InputRouteDecision &decision,
                                                           const ToolPlan &plan,
                                                           const TrustDecision &trust) const
{
    ActionSession session;
    session.id = sessionIdFor(input, decision);
    session.userRequest = input.trimmed();
    session.goal = routeGoal(decision);
    session.responseMode = chooseResponseMode(input, decision, trust);
    session.trust = trust;
    session.toolPlan = plan;
    session.selectedTools = plan.orderedToolNames;
    session.shouldAnnounceProgress = decision.kind == InputRouteKind::BackgroundTasks
        || decision.kind == InputRouteKind::DeterministicTasks
        || decision.kind == InputRouteKind::AgentConversation
        || decision.kind == InputRouteKind::CommandExtraction;

    switch (session.responseMode) {
    case ResponseMode::ActWithProgress:
        session.preamble = QStringLiteral("I'm handling that now.");
        session.progress = QStringLiteral("Working through it now.");
        session.successSummary = QStringLiteral("That task finished.");
        session.failureSummary = QStringLiteral("That task did not finish cleanly.");
        break;
    case ResponseMode::Act:
        session.preamble = QStringLiteral("I'm working through that now.");
        session.progress = QStringLiteral("Continuing the request.");
        session.successSummary = QStringLiteral("I finished that request.");
        session.failureSummary = QStringLiteral("I hit a blocker while handling that.");
        break;
    case ResponseMode::Recover:
        session.preamble = QStringLiteral("I can't complete that directly right now.");
        session.successSummary = QStringLiteral("I recovered and finished it.");
        session.failureSummary = QStringLiteral("I couldn't recover cleanly from that failure.");
        break;
    case ResponseMode::Confirm:
        session.preamble = trust.userMessage.isEmpty()
            ? QStringLiteral("I should confirm before changing anything.")
            : trust.userMessage;
        session.successSummary = QStringLiteral("Confirmed and handled.");
        session.failureSummary = QStringLiteral("I did not run that action.");
        break;
    case ResponseMode::Summarize:
        session.preamble = QStringLiteral("I'll answer directly.");
        session.successSummary = QStringLiteral("Direct response ready.");
        session.failureSummary = QStringLiteral("I couldn't produce a direct response.");
        break;
    case ResponseMode::Clarify:
        session.preamble = QStringLiteral("I need one detail to continue.");
        session.successSummary = QStringLiteral("Clarification received.");
        session.failureSummary = QStringLiteral("Still missing the detail I need.");
        break;
    case ResponseMode::Chat:
    default:
        session.preamble = QStringLiteral("I'll answer directly.");
        session.successSummary = QStringLiteral("Response ready.");
        session.failureSummary = QStringLiteral("I couldn't complete that response.");
        break;
    }

    if (!plan.rationale.isEmpty()) {
        session.nextStepHint = plan.rationale;
    }

    return session;
}

bool AssistantBehaviorPolicy::isConfirmationReply(const QString &input, const ActionSession &session) const
{
    Q_UNUSED(session)

    const QString lowered = normalizedText(input);
    if (containsAny(lowered, {
            QStringLiteral("yes"),
            QStringLiteral("confirm"),
            QStringLiteral("go ahead"),
            QStringLiteral("do it"),
            QStringLiteral("proceed"),
            QStringLiteral("continue"),
            QStringLiteral("okay"),
            QStringLiteral("ok")
        })) {
        return true;
    }

    return containsActionRestatement(lowered);
}

bool AssistantBehaviorPolicy::isRejectionReply(const QString &input) const
{
    const QString lowered = normalizedText(input);
    return containsAny(lowered, {
        QStringLiteral("no"),
        QStringLiteral("don't"),
        QStringLiteral("do not"),
        QStringLiteral("stop"),
        QStringLiteral("cancel"),
        QStringLiteral("reject"),
        QStringLiteral("never mind"),
        QStringLiteral("nevermind")
    });
}
