#include "ai/ModelCatalogService.h"

#include "ai/AiBackendClient.h"
#include "settings/AppSettings.h"

ModelCatalogService::ModelCatalogService(AppSettings *settings, AiBackendClient *client, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_client(client)
{
    connect(m_client, &AiBackendClient::modelsReady, this, [this](const QList<ModelInfo> &models) {
        m_models = models;
        if (!m_models.isEmpty() && !selectedModelValid()) {
            m_settings->setChatBackendModel(m_models.first().id);
            m_settings->save();
        }
        // modelsReady can arrive after an offline availability update; do not force online=true here.
        m_availability.modelAvailable = m_availability.online && selectedModelValid();
        if (m_availability.online) {
            const bool hasConfiguredModel = !m_settings->chatBackendModel().trimmed().isEmpty();
            if (m_models.isEmpty()) {
                m_availability.status = hasConfiguredModel
                    ? QStringLiteral("Ready (using configured model)")
                    : QStringLiteral("No models exposed by the provider");
            } else {
                m_availability.status = m_availability.modelAvailable
                    ? QStringLiteral("Ready")
                    : QStringLiteral("Selected model unavailable");
            }
        } else if (m_availability.status.trimmed().isEmpty()) {
            m_availability.status = QStringLiteral("Local AI backend offline");
        }
        emit modelsChanged();
        emit availabilityChanged();
    });

    connect(m_client, &AiBackendClient::availabilityChanged, this, [this](const AiAvailability &availability) {
        m_availability = availability;
        m_availability.modelAvailable = m_availability.online && selectedModelValid();
        if (m_availability.online && !m_models.isEmpty() && !m_availability.modelAvailable) {
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
    const QString configuredModel = m_settings->chatBackendModel().trimmed();
    if (configuredModel.isEmpty()) {
        return !m_models.isEmpty();
    }

    if (m_models.isEmpty()) {
        return true;
    }

    for (const auto &model : m_models) {
        if (model.id == configuredModel) {
            return true;
        }
    }

    // Keep manual model entries valid even when providers do not list every compatible model.
    return true;
}
