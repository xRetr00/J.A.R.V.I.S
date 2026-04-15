#pragma once

#include <QList>
#include <QString>

struct ConnectorSourceSpec
{
    QString connectorKind;
    QString fileName;
    int freshnessMs = 0;
    QString defaultPriority;
    QString capabilityId;
};

[[nodiscard]] QList<ConnectorSourceSpec> defaultConnectorSourceSpecs();
