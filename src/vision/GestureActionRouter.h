#pragma once

#include <QObject>

#include "core/AssistantTypes.h"

class LoggingService;

class GestureActionRouter final : public QObject
{
    Q_OBJECT

public:
    explicit GestureActionRouter(LoggingService *loggingService, QObject *parent = nullptr);

public slots:
    void configure(bool enabled);
    void routeGestureEvent(const GestureEvent &event);

signals:
    void gestureTriggered(const QString &gestureName, qint64 timestampMs);
    void stopSpeakingRequested();
    void cancelCurrentRequestRequested();
    void farewellRequested();
    void confirmRequested();
    void rejectRequested();

private:
    void logGestureEvent(const QString &event,
                         const QString &gestureName,
                         double confidence,
                         const QString &reason = QString(),
                         int intervalMs = 700) const;
    bool isCancelGesture(const QString &actionName, const QString &sourceGesture) const;
    bool isFarewellGesture(const QString &actionName, const QString &sourceGesture) const;
    bool isConfirmGesture(const QString &actionName, const QString &sourceGesture) const;
    bool isRejectGesture(const QString &actionName, const QString &sourceGesture) const;

    LoggingService *m_loggingService = nullptr;
    bool m_enabled = false;
};
