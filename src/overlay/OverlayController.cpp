#include "overlay/OverlayController.h"

#include <QQuickWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
{
}

void OverlayController::attachWindow(QQuickWindow *window)
{
    m_window = window;
}

bool OverlayController::isVisible() const
{
    return m_window && m_window->isVisible();
}

void OverlayController::showOverlay()
{
    if (!m_window) {
        return;
    }
    m_window->show();
    emit visibilityChanged(true);
}

void OverlayController::hideOverlay()
{
    if (!m_window) {
        return;
    }
    m_window->hide();
    emit visibilityChanged(false);
}

void OverlayController::toggleOverlay()
{
    if (isVisible()) {
        hideOverlay();
    } else {
        showOverlay();
    }
}

void OverlayController::setClickThrough(bool enabled)
{
    if (!m_window) {
        return;
    }

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enabled) {
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
#else
    Q_UNUSED(enabled)
#endif
}
