#pragma once

#include <QObject>

class QQuickWindow;

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

signals:
    void visibilityChanged(bool visible);

private:
    QQuickWindow *m_window = nullptr;
};
