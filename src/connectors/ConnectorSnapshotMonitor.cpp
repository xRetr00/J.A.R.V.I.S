#include "connectors/ConnectorSnapshotMonitor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>

#include "connectors/ConnectorSnapshotEventBuilder.h"
#include "connectors/ConnectorSourceSpec.h"
#include "logging/LoggingService.h"
#include "settings/AppSettings.h"

namespace {
constexpr int kPollIntervalMs = 4000;
}

ConnectorSnapshotMonitor::ConnectorSnapshotMonitor(AppSettings *settings,
                                                   LoggingService *loggingService,
                                                   QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
    , m_pollTimer(new QTimer(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_sources(defaultConnectorSourceSpecs())
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &ConnectorSnapshotMonitor::pollSnapshots);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &ConnectorSnapshotMonitor::handleDirectoryChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ConnectorSnapshotMonitor::handleFileChanged);
}

void ConnectorSnapshotMonitor::start()
{
    QDir().mkpath(snapshotRootPath());
    ensureWatchTargets();
    pollSnapshots();
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
}

void ConnectorSnapshotMonitor::stop()
{
    m_pollTimer->stop();
    m_fileWatcher->removePaths(m_fileWatcher->files());
    m_fileWatcher->removePaths(m_fileWatcher->directories());
}

QString ConnectorSnapshotMonitor::snapshotRootPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/connectors");
}

void ConnectorSnapshotMonitor::ensureWatchTargets()
{
    const QString root = snapshotRootPath();
    if (!m_fileWatcher->directories().contains(root)) {
        m_fileWatcher->addPath(root);
    }

    m_sourceByPath.clear();
    for (const ConnectorSourceSpec &source : m_sources) {
        const QString path = snapshotPathFor(source);
        m_sourceByPath.insert(path, source);
        if (QFileInfo::exists(path) && !m_fileWatcher->files().contains(path)) {
            m_fileWatcher->addPath(path);
        }
    }
}

void ConnectorSnapshotMonitor::handleDirectoryChanged(const QString &path)
{
    if (path != snapshotRootPath()) {
        return;
    }
    ensureWatchTargets();
    pollSnapshots();
}

void ConnectorSnapshotMonitor::handleFileChanged(const QString &path)
{
    const auto it = m_sourceByPath.constFind(path);
    if (it == m_sourceByPath.cend()) {
        return;
    }

    ensureWatchTargets();
    if (m_settings != nullptr && m_settings->privateModeEnabled()) {
        return;
    }

    pollSource(it.value(), QDateTime::currentDateTimeUtc());
}

void ConnectorSnapshotMonitor::pollSnapshots()
{
    if (m_settings != nullptr && m_settings->privateModeEnabled()) {
        return;
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    for (const ConnectorSourceSpec &source : m_sources) {
        pollSource(source, nowUtc);
    }
}

QString ConnectorSnapshotMonitor::snapshotPathFor(const ConnectorSourceSpec &source) const
{
    return snapshotRootPath() + QStringLiteral("/") + source.fileName;
}

void ConnectorSnapshotMonitor::pollSource(const ConnectorSourceSpec &source, const QDateTime &nowUtc)
{
    const QString path = snapshotPathFor(source);
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        m_lastDigestByPath.remove(path);
        if (m_fileWatcher->files().contains(path)) {
            m_fileWatcher->removePath(path);
        }
        return;
    }

    if (source.freshnessMs > 0
        && info.lastModified().toUTC().msecsTo(nowUtc) > source.freshnessMs) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray content = file.readAll();
    file.close();
    ConnectorEvent event = ConnectorSnapshotEventBuilder::fromSnapshot(
        source.connectorKind,
        path,
        content,
        info.lastModified().toUTC(),
        source.defaultPriority);
    if (!event.isValid()) {
        return;
    }

    event.sourceKind = QStringLiteral("connector_%1_poller").arg(source.connectorKind);
    event.metadata.insert(QStringLiteral("producer"), source.capabilityId);
    event.metadata.insert(QStringLiteral("sourceFileName"), source.fileName);
    event.metadata.insert(QStringLiteral("freshnessMs"), source.freshnessMs);

    const QString digest = event.metadata.value(QStringLiteral("snapshotDigest")).toString();
    if (digest.isEmpty() || m_lastDigestByPath.value(path) == digest) {
        return;
    }

    m_lastDigestByPath.insert(path, digest);
    if (m_loggingService != nullptr) {
        m_loggingService->infoFor(
            QStringLiteral("tools_mcp"),
            QStringLiteral("[ConnectorSnapshotMonitor] detected %1 from %2")
                .arg(source.connectorKind, source.fileName));
    }
    emit connectorEventDetected(event);
}
