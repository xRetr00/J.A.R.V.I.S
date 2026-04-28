#pragma once

#include <memory>

#include <QObject>

#include "smart_home/SmartHomeTypes.h"
#include "smart_home/SmartRoomBehaviorPolicy.h"
#include "smart_home/SmartRoomStateMachine.h"

class AppSettings;
class DesktopBleIdentityAdapter;
class HomeAssistantSmartHomeAdapter;
class LoggingService;
class QTimer;

class SmartHomeRuntime final : public QObject
{
    Q_OBJECT

public:
    SmartHomeRuntime(AppSettings *settings, LoggingService *loggingService, QObject *parent = nullptr);

    void start();
    void stop();
    SmartHomeSnapshot latestSnapshot() const;
    static SmartHomeSnapshot latestSharedSnapshot();

signals:
    void roomTransitionReady(const SmartRoomTransition &transition);
    void welcomeRequested(const QString &message, const SmartWelcomeDecision &decision);
    void statusChanged(const QString &status, bool available);

private slots:
    void reconfigureFromSettings();
    void pollState();

private:
    SmartHomeConfig configFromSettings() const;
    void handleSnapshot(const SmartHomeSnapshot &snapshot);
    void reconfigureIdentityAdapter();
    void logTransition(const SmartRoomTransition &transition) const;
    void logWelcomeDecision(const SmartWelcomeDecision &decision, const SmartRoomTransition &transition) const;

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    QTimer *m_pollTimer = nullptr;
    HomeAssistantSmartHomeAdapter *m_adapter = nullptr;
    DesktopBleIdentityAdapter *m_identityAdapter = nullptr;
    SmartRoomStateMachine m_stateMachine;
    SmartRoomBehaviorPolicy m_behaviorPolicy;
    SmartHomeConfig m_config;
    SmartHomeSnapshot m_latestSnapshot;
    qint64 m_lastWelcomeAtMs = 0;
    bool m_pollInFlight = false;
};
