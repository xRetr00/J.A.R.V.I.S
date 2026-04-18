#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "companion/contracts/ActionProposal.h"

class SuggestionProposalBuilder
{
public:
    struct Input
    {
        QString sourceKind;
        QString taskType;
        QString resultSummary;
        QStringList sourceUrls;
        QVariantMap sourceMetadata;
        bool success = true;
    };

    [[nodiscard]] static QList<ActionProposal> build(const Input &input);

private:
    static void appendUnique(QList<ActionProposal> &proposals, const ActionProposal &proposal);
    [[nodiscard]] static ActionProposal makeProposal(const QString &capabilityId,
                                                     const QString &title,
                                                     const QString &summary,
                                                     const QString &priority,
                                                     const Input &input);
};
