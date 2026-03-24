#pragma once

#include <QObject>
#include <QString>

class QQuickWindow;
class QPropertyAnimation;
class QTimer;

class OverlayController : public QObject
{
    Q_OBJECT

public:
    explicit OverlayController(QObject *parent = nullptr);

    void attachWindow(QQuickWindow *window);
    bool isVisible() const;
    void showOverlay();
    void hideOverlay();
    void toggleOverlay();
    void setClickThrough(bool enabled);
    void setAssistantState(const QString &stateName);
    void setSetupVisible(bool visible);

signals:
    void visibilityChanged(bool visible);

private:
    void reevaluateVisibility();
    void animateToVisible(bool visible);
    void positionWindow() const;
    bool isUserActive() const;
    bool isFullscreenForeground() const;
    qint64 nowMs() const;

    QQuickWindow *m_window = nullptr;
    QPropertyAnimation *m_opacityAnimation = nullptr;
    QTimer *m_monitorTimer = nullptr;
    bool m_visible = false;
    bool m_manualRequested = false;
    bool m_assistantBusy = false;
    bool m_assistantSpeaking = false;
    bool m_setupVisible = false;
    qint64 m_ignoreUserUntilMs = 0;
};
