#include "connectors/NotesDirectoryMonitor.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>

#include "connectors/NotesDirectoryEventBuilder.h"

namespace {
constexpr int kPollIntervalMs = 5000;
}

NotesDirectoryMonitor::NotesDirectoryMonitor(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &NotesDirectoryMonitor::pollFiles);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &NotesDirectoryMonitor::handleDirectoryChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &NotesDirectoryMonitor::handleFileChanged);
}

void NotesDirectoryMonitor::start()
{
    QDir().mkpath(notesRootPath());
    ensureWatchTargets();
    pollFiles();
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
}

void NotesDirectoryMonitor::stop()
{
    m_pollTimer->stop();
    m_fileWatcher->removePaths(m_fileWatcher->files());
    m_fileWatcher->removePaths(m_fileWatcher->directories());
}

QString NotesDirectoryMonitor::notesRootPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/Vaxil Notes");
}

void NotesDirectoryMonitor::ensureWatchTargets()
{
    const QString root = notesRootPath();
    if (!m_fileWatcher->directories().contains(root)) {
        m_fileWatcher->addPath(root);
    }

    const QDir dir(root);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.md"), QStringLiteral("*.txt")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Time);
    for (const QFileInfo &info : files) {
        const QString path = info.absoluteFilePath();
        if (!m_fileWatcher->files().contains(path)) {
            m_fileWatcher->addPath(path);
        }
    }
}

void NotesDirectoryMonitor::handleDirectoryChanged(const QString &path)
{
    if (path != notesRootPath()) {
        return;
    }
    ensureWatchTargets();
    pollFiles();
}

void NotesDirectoryMonitor::handleFileChanged(const QString &path)
{
    Q_UNUSED(path)
    ensureWatchTargets();
    pollFiles();
}

void NotesDirectoryMonitor::pollFiles()
{
    const QDir dir(notesRootPath());
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.md"), QStringLiteral("*.txt")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Time);
    for (const QFileInfo &info : files) {
        const ConnectorEvent event = NotesDirectoryEventBuilder::fromFile(
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
