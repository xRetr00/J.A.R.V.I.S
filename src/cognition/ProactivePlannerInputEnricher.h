#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

class ProactivePlannerInputEnricher
{
public:
    struct Input
    {
        QString sourceKind;
        QString taskType;
        QString resultSummary;
        QStringList sourceUrls;
        QVariantMap sourceMetadata;
        QVariantMap desktopContext;
        qint64 nowMs = 0;
        bool success = true;
    };

    [[nodiscard]] static QVariantMap enrich(const Input &input);
};
