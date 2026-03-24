#pragma once

#include <QObject>
#include <QStringList>

#include "core/AssistantTypes.h"

class ReasoningRouter : public QObject
{
    Q_OBJECT

public:
    explicit ReasoningRouter(QObject *parent = nullptr);

    ReasoningMode chooseMode(const QString &input, bool autoRoutingEnabled, ReasoningMode defaultMode) const;
    bool isLikelyCommand(const QString &input) const;
    bool isDeepAnalysis(const QString &input) const;

private:
    QStringList m_commandKeywords;
    QStringList m_analysisKeywords;
};
