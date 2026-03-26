#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class BackendFacade;

class TaskViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList backgroundTaskResults READ backgroundTaskResults NOTIFY backgroundTaskResultsChanged)
    Q_PROPERTY(bool backgroundPanelVisible READ backgroundPanelVisible NOTIFY backgroundPanelVisibleChanged)
    Q_PROPERTY(QString latestTaskToast READ latestTaskToast NOTIFY latestTaskToastChanged)
    Q_PROPERTY(QString latestTaskToastTone READ latestTaskToastTone NOTIFY latestTaskToastChanged)
    Q_PROPERTY(int latestTaskToastTaskId READ latestTaskToastTaskId NOTIFY latestTaskToastChanged)
    Q_PROPERTY(QString latestTaskToastType READ latestTaskToastType NOTIFY latestTaskToastChanged)

public:
    explicit TaskViewModel(BackendFacade *backend, QObject *parent = nullptr);

    QVariantList backgroundTaskResults() const;
    bool backgroundPanelVisible() const;
    QString latestTaskToast() const;
    QString latestTaskToastTone() const;
    int latestTaskToastTaskId() const;
    QString latestTaskToastType() const;

    Q_INVOKABLE void setBackgroundPanelVisible(bool visible);
    Q_INVOKABLE void notifyTaskToastShown(int taskId);
    Q_INVOKABLE void notifyTaskPanelRendered();

signals:
    void backgroundTaskResultsChanged();
    void backgroundPanelVisibleChanged();
    void latestTaskToastChanged();

private:
    BackendFacade *m_backend = nullptr;
};
