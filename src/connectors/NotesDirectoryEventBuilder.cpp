#include "connectors/NotesDirectoryEventBuilder.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
QString firstMeaningfulLine(const QString &text)
{
    const QStringList lines = text.split(QChar::fromLatin1('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        while (line.startsWith(QChar::fromLatin1('#'))) {
            line.remove(0, 1);
            line = line.trimmed();
        }
        if (!line.isEmpty()) {
            return line;
        }
    }
    return {};
}
}

ConnectorEvent NotesDirectoryEventBuilder::fromFile(const QString &filePath,
                                                    const QDateTime &lastModifiedUtc,
                                                    const QString &defaultPriority)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return {};
    }

    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix != QStringLiteral("md") && suffix != QStringLiteral("txt")) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    const QString text = stream.readAll().left(4096);
    file.close();
    const QString title = firstMeaningfulLine(text);
    if (title.isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(
        (info.absoluteFilePath() + QStringLiteral("|") + title).toUtf8(),
        QCryptographicHash::Sha1).toHex();

    ConnectorEvent event;
    event.eventId = QString::fromLatin1(digest.left(16));
    event.sourceKind = QStringLiteral("connector_notes_directory");
    event.connectorKind = QStringLiteral("notes");
    event.taskType = QStringLiteral("note_review");
    event.summary = QStringLiteral("Notes updated: %1").arg(title);
    event.taskKey = QStringLiteral("notes:%1").arg(info.fileName());
    event.itemCount = 1;
    event.priority = defaultPriority.trimmed().isEmpty() ? QStringLiteral("low") : defaultPriority.trimmed().toLower();
    event.occurredAtUtc = lastModifiedUtc.isValid() ? lastModifiedUtc : QDateTime::currentDateTimeUtc();
    event.metadata = {
        {QStringLiteral("producer"), QStringLiteral("connector_notes_directory_monitor")},
        {QStringLiteral("filePath"), info.absoluteFilePath()},
        {QStringLiteral("fileName"), info.fileName()},
        {QStringLiteral("title"), title},
        {QStringLiteral("suffix"), suffix}
    };
    return event;
}
