#pragma once

#include <QHash>
#include <QObject>

#include "companion/contracts/ConnectorEvent.h"

class QFileSystemWatcher;
class QTimer;

class BrowserBookmarksMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BrowserBookmarksMonitor(QObject *parent = nullptr);

    void start();
    void stop();

signals:
    void connectorEventDetected(const ConnectorEvent &event);

private:
    struct BookmarkSource {
        QString browserName;
        QString filePath;
        QString priority;
    };

    void ensureWatchTargets();
    void handleFileChanged(const QString &path);
    void pollFiles();
    [[nodiscard]] QList<BookmarkSource> bookmarkSources() const;

    QTimer *m_pollTimer = nullptr;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QHash<QString, QString> m_lastEventIdByPath;
};
