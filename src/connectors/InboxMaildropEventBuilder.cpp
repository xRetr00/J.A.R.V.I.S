#include "connectors/InboxMaildropEventBuilder.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
QString headerValue(const QStringList &lines, const QString &headerName)
{
    const QString prefix = headerName + QStringLiteral(":");
    for (const QString &line : lines) {
        if (line.startsWith(prefix, Qt::CaseInsensitive)) {
            return line.mid(prefix.size()).trimmed();
        }
    }
    return {};
}

QString firstBodyLine(const QStringList &lines)
{
    bool inBody = false;
    for (QString line : lines) {
        if (!inBody) {
            if (line.trimmed().isEmpty()) {
                inBody = true;
            }
            continue;
        }

        line = line.trimmed();
        if (!line.isEmpty()) {
            return line;
        }
    }
    return {};
}

QString effectiveSubject(const QStringList &lines)
{
    const QString subject = headerValue(lines, QStringLiteral("Subject"));
    if (!subject.isEmpty()) {
        return subject;
    }
    return firstBodyLine(lines);
}
}

ConnectorEvent InboxMaildropEventBuilder::fromFile(const QString &filePath,
                                                   const QDateTime &lastModifiedUtc,
                                                   const QString &defaultPriority)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return {};
    }

    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix != QStringLiteral("eml") && suffix != QStringLiteral("txt")) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    const QString content = stream.readAll().left(8192);
    file.close();
    const QStringList lines = content.split(QChar::fromLatin1('\n'));
    const QString sender = headerValue(lines, QStringLiteral("From"));
    const QString subject = effectiveSubject(lines).trimmed();
    if (subject.isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(
        (info.absoluteFilePath()
         + QChar::fromLatin1('|')
         + sender
         + QChar::fromLatin1('|')
         + subject).toUtf8(),
        QCryptographicHash::Sha1).toHex();

    ConnectorEvent event;
    event.eventId = QString::fromLatin1(digest.left(16));
    event.sourceKind = QStringLiteral("connector_inbox_maildrop");
    event.connectorKind = QStringLiteral("inbox");
    event.taskType = QStringLiteral("email_fetch");
    event.summary = sender.isEmpty()
        ? QStringLiteral("Inbox updated: %1").arg(subject)
        : QStringLiteral("Inbox updated: %1 from %2").arg(subject, sender);
    event.taskKey = QStringLiteral("inbox:%1").arg(info.fileName());
    event.itemCount = 1;
    event.priority = defaultPriority.trimmed().isEmpty() ? QStringLiteral("high") : defaultPriority.trimmed().toLower();
    event.occurredAtUtc = lastModifiedUtc.isValid() ? lastModifiedUtc : QDateTime::currentDateTimeUtc();
    event.metadata = {
        {QStringLiteral("producer"), QStringLiteral("connector_inbox_maildrop_monitor")},
        {QStringLiteral("filePath"), info.absoluteFilePath()},
        {QStringLiteral("fileName"), info.fileName()},
        {QStringLiteral("sender"), sender},
        {QStringLiteral("subject"), subject},
        {QStringLiteral("suffix"), suffix}
    };
    return event;
}
