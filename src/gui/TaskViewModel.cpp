#include "gui/TaskViewModel.h"

#include "gui/BackendFacade.h"

TaskViewModel::TaskViewModel(BackendFacade *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
    if (!m_backend) {
        return;
    }

    connect(m_backend, &BackendFacade::backgroundTaskResultsChanged, this, &TaskViewModel::backgroundTaskResultsChanged);
    connect(m_backend, &BackendFacade::backgroundPanelVisibleChanged, this, &TaskViewModel::backgroundPanelVisibleChanged);
    connect(m_backend, &BackendFacade::latestTaskToastChanged, this, &TaskViewModel::latestTaskToastChanged);
}

QVariantList TaskViewModel::backgroundTaskResults() const
{
    return m_backend ? m_backend->backgroundTaskResults() : QVariantList();
}

bool TaskViewModel::backgroundPanelVisible() const
{
    return m_backend && m_backend->backgroundPanelVisible();
}

QString TaskViewModel::latestTaskToast() const
{
    return m_backend ? m_backend->latestTaskToast() : QString();
}

QString TaskViewModel::latestTaskToastTone() const
{
    return m_backend ? m_backend->latestTaskToastTone() : QStringLiteral("status");
}

int TaskViewModel::latestTaskToastTaskId() const
{
    return m_backend ? m_backend->latestTaskToastTaskId() : -1;
}

QString TaskViewModel::latestTaskToastType() const
{
    return m_backend ? m_backend->latestTaskToastType() : QStringLiteral("status");
}

void TaskViewModel::setBackgroundPanelVisible(bool visible)
{
    if (m_backend) {
        m_backend->setBackgroundPanelVisible(visible);
    }
}

void TaskViewModel::notifyTaskToastShown(int taskId)
{
    if (m_backend) {
        m_backend->notifyTaskToastShown(taskId);
    }
}

void TaskViewModel::notifyTaskPanelRendered()
{
    if (m_backend) {
        m_backend->notifyTaskPanelRendered();
    }
}
