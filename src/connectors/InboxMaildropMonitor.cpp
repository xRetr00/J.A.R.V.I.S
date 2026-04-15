#include "connectors/InboxMaildropMonitor.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>

#include "connectors/InboxMaildropEventBuilder.h"

namespace {
constexpr int kPollIntervalMs = 5000;
}

InboxMaildropMonitor::InboxMaildropMonitor(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &InboxMaildropMonitor::pollFiles);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &InboxMaildropMonitor::handleDirectoryChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &InboxMaildropMonitor::handleFileChanged);
}

void InboxMaildropMonitor::start()
{
    QDir().mkpath(inboxRootPath());
    ensureWatchTargets();
    pollFiles();
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
}

void InboxMaildropMonitor::stop()
{
    m_pollTimer->stop();
    m_fileWatcher->removePaths(m_fileWatcher->files());
    m_fileWatcher->removePaths(m_fileWatcher->directories());
}

QString InboxMaildropMonitor::inboxRootPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/Vaxil Inbox");
}

void InboxMaildropMonitor::ensureWatchTargets()
{
    const QString root = inboxRootPath();
    if (!m_fileWatcher->directories().contains(root)) {
        m_fileWatcher->addPath(root);
    }

    const QDir dir(root);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.eml"), QStringLiteral("*.txt")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Time);
    for (const QFileInfo &info : files) {
        const QString path = info.absoluteFilePath();
        if (!m_fileWatcher->files().contains(path)) {
            m_fileWatcher->addPath(path);
        }
    }
}

void InboxMaildropMonitor::handleDirectoryChanged(const QString &path)
{
    if (path != inboxRootPath()) {
        return;
    }
    ensureWatchTargets();
    pollFiles();
}

void InboxMaildropMonitor::handleFileChanged(const QString &path)
{
    Q_UNUSED(path)
    ensureWatchTargets();
    pollFiles();
}

void InboxMaildropMonitor::pollFiles()
{
    const QDir dir(inboxRootPath());
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.eml"), QStringLiteral("*.txt")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Time);
    for (const QFileInfo &info : files) {
        const ConnectorEvent event = InboxMaildropEventBuilder::fromFile(
            info.absoluteFilePath(),
            info.lastModified().toUTC());
        if (!event.isValid()) {
            continue;
        }

        const QString path = info.absoluteFilePath();
        if (m_lastEventIdByPath.value(path) == event.eventId) {
            continue;
        }

        m_lastEventIdByPath.insert(path, event.eventId);
        emit connectorEventDetected(event);
    }
}
