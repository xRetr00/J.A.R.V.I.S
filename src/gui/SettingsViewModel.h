#pragma once

#include <QObject>
#include <QStringList>

class BackendFacade;

class SettingsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(QString selectedModel READ selectedModel NOTIFY selectedModelChanged)
    Q_PROPERTY(QString lmStudioEndpoint READ lmStudioEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(int defaultReasoningMode READ defaultReasoningMode NOTIFY settingsChanged)
    Q_PROPERTY(bool autoRoutingEnabled READ autoRoutingEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool streamingEnabled READ streamingEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int requestTimeoutMs READ requestTimeoutMs NOTIFY settingsChanged)
    Q_PROPERTY(bool clickThroughEnabled READ clickThroughEnabled NOTIFY settingsChanged)

public:
    explicit SettingsViewModel(BackendFacade *backend, QObject *parent = nullptr);

    QStringList models() const;
    QString selectedModel() const;
    QString lmStudioEndpoint() const;
    int defaultReasoningMode() const;
    bool autoRoutingEnabled() const;
    bool streamingEnabled() const;
    int requestTimeoutMs() const;
    bool clickThroughEnabled() const;

    Q_INVOKABLE void refreshModels();
    Q_INVOKABLE void setSelectedModel(const QString &modelId);

signals:
    void modelsChanged();
    void selectedModelChanged();
    void settingsChanged();

private:
    BackendFacade *m_backend = nullptr;
};
