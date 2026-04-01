#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

#include "core/AssistantTypes.h"

class LocalResponseEngine : public QObject
{
    Q_OBJECT

public:
    explicit LocalResponseEngine(QObject *parent = nullptr);

    bool initialize();
    QString respondToIntent(LocalIntent intent, const LocalResponseContext &context, ResponseMode mode = ResponseMode::Chat);
    QString respondToError(const QString &errorKey, const LocalResponseContext &context, ResponseMode mode = ResponseMode::Recover);
    QString acknowledgement(const QString &target, const LocalResponseContext &context, ResponseMode mode = ResponseMode::Act);
    QString wakeWordReady(const LocalResponseContext &context, ResponseMode mode = ResponseMode::Summarize);
    QString currentTimeResponse(const LocalResponseContext &context, ResponseMode mode = ResponseMode::Summarize);
    QString currentDateResponse(const LocalResponseContext &context, ResponseMode mode = ResponseMode::Summarize);

private:
    QString resolveGroup(LocalIntent intent, const LocalResponseContext &context) const;
    QString renderTemplate(const QString &variant, const LocalResponseContext &context, const QString &target = QString()) const;
    QString chooseVariant(const QString &group);
    QString conciseResponseForMode(ResponseMode mode, const QString &defaultText) const;

    QHash<QString, QStringList> m_responses;
    QHash<QString, int> m_lastIndexByGroup;
};
