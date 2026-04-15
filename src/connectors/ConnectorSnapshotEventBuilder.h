#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include "companion/contracts/ConnectorEvent.h"

class ConnectorSnapshotEventBuilder
{
public:
    [[nodiscard]] static ConnectorEvent fromSnapshot(const QString &connectorKind,
                                                     const QString &snapshotPath,
                                                     const QByteArray &content,
                                                     const QDateTime &lastModifiedUtc,
                                                     const QString &defaultPriority = {});
    [[nodiscard]] static ConnectorEvent fromSnapshot(const QString &snapshotPath,
                                                     const QByteArray &content,
                                                     const QDateTime &lastModifiedUtc);
};
