#include "gui/SettingsViewModel.h"

#include "gui/BackendFacade.h"

SettingsViewModel::SettingsViewModel(BackendFacade *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
    if (!m_backend) {
        return;
    }

    connect(m_backend, &BackendFacade::modelsChanged, this, &SettingsViewModel::modelsChanged);
    connect(m_backend, &BackendFacade::selectedModelChanged, this, &SettingsViewModel::selectedModelChanged);
    connect(m_backend, &BackendFacade::settingsChanged, this, &SettingsViewModel::settingsChanged);
}

QStringList SettingsViewModel::models() const
{
    return m_backend ? m_backend->models() : QStringList();
}

QString SettingsViewModel::selectedModel() const
{
    return m_backend ? m_backend->selectedModel() : QString();
}

QString SettingsViewModel::lmStudioEndpoint() const
{
    return m_backend ? m_backend->lmStudioEndpoint() : QString();
}

int SettingsViewModel::defaultReasoningMode() const
{
    return m_backend ? m_backend->defaultReasoningMode() : 1;
}

bool SettingsViewModel::autoRoutingEnabled() const
{
    return m_backend && m_backend->autoRoutingEnabled();
}

bool SettingsViewModel::streamingEnabled() const
{
    return m_backend && m_backend->streamingEnabled();
}

int SettingsViewModel::requestTimeoutMs() const
{
    return m_backend ? m_backend->requestTimeoutMs() : 120000;
}

bool SettingsViewModel::clickThroughEnabled() const
{
    return m_backend && m_backend->clickThroughEnabled();
}

void SettingsViewModel::refreshModels()
{
    if (m_backend) {
        m_backend->refreshModels();
    }
}

void SettingsViewModel::setSelectedModel(const QString &modelId)
{
    if (m_backend) {
        m_backend->setSelectedModel(modelId);
    }
}
