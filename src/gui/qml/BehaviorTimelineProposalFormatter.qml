import QtQml

QtObject {
    function proposalEvidence(entry) {
        const sourceLabel = (entry.sourceLabel || "").toString()
        const keyHint = (entry.presentationKeyHint || "").toString()
        const proposalReason = (entry.proposalReasonCode || "").toString()
        const parts = []
        if (sourceLabel.length > 0) {
            parts.push("source " + sourceLabel)
        }
        if (keyHint.length > 0) {
            parts.push("key " + keyHint)
        }
        if (proposalReason.length > 0) {
            parts.push("reason " + proposalReason)
        }
        return parts.join(" | ")
    }

    function summaryText(entry) {
        const stage = (entry.stage || "").toString()
        const title = (entry.title || "").toString()
        const action = (entry.action || "").toString()
        const priority = (entry.priority || "").toString()
        const reasonCode = (entry.reasonCode || "").toString()
        const evidence = proposalEvidence(entry)

        if (stage === "generated") {
            const proposalCount = entry.proposalCount || 0
            const proposalTitles = entry.proposalTitles || []
            return "Generated " + proposalCount + " suggestion proposals: " + proposalTitles.join(", ")
        }

        if (stage === "ranked") {
            const rankedTitles = entry.rankedTitles || []
            let text = "Ranked suggestion proposals"
            if (rankedTitles.length > 0) {
                text += ": " + rankedTitles.join(" > ")
            }
            if (evidence.length > 0) {
                text += " | " + evidence
            }
            if (reasonCode.length > 0) {
                text += " because " + reasonCode
            }
            return text
        }

        if (stage === "gated") {
            const confidenceScore = entry.confidenceScore
            const noveltyScore = entry.noveltyScore
            let text = "Proposal " + (action.length > 0 ? action : "decision")
            if (title.length > 0) {
                text += ": " + title
            }
            if (priority.length > 0) {
                text += " [" + priority + "]"
            }
            if (confidenceScore !== undefined && confidenceScore !== null) {
                text += " confidence " + Number(confidenceScore).toFixed(2)
            }
            if (noveltyScore !== undefined && noveltyScore !== null) {
                text += " novelty " + Number(noveltyScore).toFixed(2)
            }
            if (evidence.length > 0) {
                text += " | " + evidence
            }
            if (reasonCode.length > 0) {
                text += " because " + reasonCode
            }
            return text
        }

        let text = "Proposal " + (action.length > 0 ? action : "decision")
        if (title.length > 0) {
            text += ": " + title
        }
        if (priority.length > 0) {
            text += " [" + priority + "]"
        }
        if (evidence.length > 0) {
            text += " | " + evidence
        }
        if (reasonCode.length > 0) {
            text += " because " + reasonCode
        }
        return text
    }
}
