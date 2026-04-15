#pragma once

#include <QHash>
#include <QObject>

#include "companion/contracts/ConnectorEvent.h"
#include "connectors/ConnectorSourceSpec.h"

class AppSettings;
class QFileSystemWatcher;
class LoggingService;
class QTimer;

class ConnectorSnapshotMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ConnectorSnapshotMonitor(AppSettings *settings = nullptr,
                                      LoggingService *loggingService = nullptr,
                                      QObject *parent = nullptr);

    void start();
    void stop();
    [[nodiscard]] QString snapshotRootPath() const;

signals:
    void connectorEventDetected(const ConnectorEvent &event);

private:
    void ensureWatchTargets();
    void handleDirectoryChanged(const QString &path);
    void handleFileChanged(const QString &path);
    void pollSnapshots();
    void pollSource(const ConnectorSourceSpec &source, const QDateTime &nowUtc);
    [[nodiscard]] QString snapshotPathFor(const ConnectorSourceSpec &source) const;

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    QTimer *m_pollTimer = nullptr;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QList<ConnectorSourceSpec> m_sources;
    QHash<QString, ConnectorSourceSpec> m_sourceByPath;
    QHash<QString, QString> m_lastDigestByPath;
};
