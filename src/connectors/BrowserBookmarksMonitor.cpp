#include "connectors/BrowserBookmarksMonitor.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

#include "connectors/BrowserBookmarksEventBuilder.h"

namespace {
constexpr int kPollIntervalMs = 15000;

QString localAppDataPath()
{
    const QString env = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (!env.isEmpty()) {
        return env;
    }
    return {};
}
}

BrowserBookmarksMonitor::BrowserBookmarksMonitor(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &BrowserBookmarksMonitor::pollFiles);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &BrowserBookmarksMonitor::handleFileChanged);
}

void BrowserBookmarksMonitor::start()
{
    ensureWatchTargets();
    pollFiles();
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
}

void BrowserBookmarksMonitor::stop()
{
    m_pollTimer->stop();
    m_fileWatcher->removePaths(m_fileWatcher->files());
}

QList<BrowserBookmarksMonitor::BookmarkSource> BrowserBookmarksMonitor::bookmarkSources() const
{
    const QString localAppData = localAppDataPath();
    if (localAppData.isEmpty()) {
        return {};
    }

    return {
        {
            .browserName = QStringLiteral("Edge"),
            .filePath = localAppData + QStringLiteral("/Microsoft/Edge/User Data/Default/Bookmarks"),
            .priority = QStringLiteral("medium")
        },
        {
            .browserName = QStringLiteral("Chrome"),
            .filePath = localAppData + QStringLiteral("/Google/Chrome/User Data/Default/Bookmarks"),
            .priority = QStringLiteral("medium")
        },
        {
            .browserName = QStringLiteral("Brave"),
            .filePath = localAppData + QStringLiteral("/BraveSoftware/Brave-Browser/User Data/Default/Bookmarks"),
            .priority = QStringLiteral("medium")
        }
    };
}

void BrowserBookmarksMonitor::ensureWatchTargets()
{
    for (const BookmarkSource &source : bookmarkSources()) {
        if (QFileInfo::exists(source.filePath) && !m_fileWatcher->files().contains(source.filePath)) {
            m_fileWatcher->addPath(source.filePath);
        }
    }
}

void BrowserBookmarksMonitor::handleFileChanged(const QString &path)
{
    Q_UNUSED(path)
    ensureWatchTargets();
    pollFiles();
}

void BrowserBookmarksMonitor::pollFiles()
{
    for (const BookmarkSource &source : bookmarkSources()) {
        const QFileInfo info(source.filePath);
        if (!info.exists() || !info.isFile() || !info.isReadable()) {
            m_lastEventIdByPath.remove(source.filePath);
            continue;
        }

        const ConnectorEvent event = BrowserBookmarksEventBuilder::fromBookmarksFile(
            source.filePath,
            source.browserName,
            info.lastModified().toUTC(),
            source.priority);
        if (!event.isValid()) {
            continue;
        }

        if (m_lastEventIdByPath.value(source.filePath) == event.eventId) {
            continue;
        }

        m_lastEventIdByPath.insert(source.filePath, event.eventId);
        emit connectorEventDetected(event);
    }
}
