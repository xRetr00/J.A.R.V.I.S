#include "ai/ModelCatalogService.h"

#include "ai/LmStudioClient.h"
#include "settings/AppSettings.h"

ModelCatalogService::ModelCatalogService(AppSettings *settings, LmStudioClient *client, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_client(client)
{
    connect(m_client, &LmStudioClient::modelsReady, this, [this](const QList<ModelInfo> &models) {
        m_models = models;
        if (m_settings->selectedModel().isEmpty() && !m_models.isEmpty()) {
            m_settings->setSelectedModel(m_models.first().id);
            m_settings->save();
        }
        m_availability.online = true;
        m_availability.modelAvailable = selectedModelValid();
        m_availability.status = m_models.isEmpty()
            ? QStringLiteral("No models exposed by LM Studio")
            : (m_availability.modelAvailable ? QStringLiteral("Ready") : QStringLiteral("Selected model unavailable"));
        emit modelsChanged();
        emit availabilityChanged();
    });

    connect(m_client, &LmStudioClient::availabilityChanged, this, [this](const AiAvailability &availability) {
        m_availability = availability;
        m_availability.modelAvailable = selectedModelValid();
        if (m_availability.online && !m_availability.modelAvailable) {
            m_availability.status = QStringLiteral("Selected model unavailable");
        }
        emit availabilityChanged();
    });
}

QList<ModelInfo> ModelCatalogService::models() const
{
    return m_models;
}

AiAvailability ModelCatalogService::availability() const
{
    return m_availability;
}

void ModelCatalogService::refresh()
{
    m_client->fetchModels();
}

bool ModelCatalogService::selectedModelValid() const
{
    if (m_settings->selectedModel().isEmpty()) {
        return !m_models.isEmpty();
    }

    for (const auto &model : m_models) {
        if (model.id == m_settings->selectedModel()) {
            return true;
        }
    }

    return false;
}
