#pragma once

#include <QObject>
#include <QStringList>

#include "core/AssistantTypes.h"

class IntentRouter : public QObject
{
    Q_OBJECT

public:
    explicit IntentRouter(QObject *parent = nullptr);

    LocalIntent classify(const QString &input) const;

private:
    bool containsKeyword(const QString &input, const QString &keyword) const;
    bool startsWithKeyword(const QString &input, const QString &keyword) const;

    QStringList m_greetingKeywords;
    QStringList m_smallTalkKeywords;
    QStringList m_commandKeywords;
};
