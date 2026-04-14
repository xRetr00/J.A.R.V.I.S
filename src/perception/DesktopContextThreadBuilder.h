#pragma once

#include <QString>
#include <QVariantMap>

#include "companion/contracts/CompanionContextSnapshot.h"

class DesktopContextThreadBuilder
{
public:
    [[nodiscard]] static CompanionContextSnapshot fromActiveWindow(const QString &appId,
                                                                   const QString &windowTitle,
                                                                   const QVariantMap &externalMetadata = {});
    [[nodiscard]] static CompanionContextSnapshot fromClipboard(const QString &appId,
                                                                const QString &windowTitle,
                                                                const QString &clipboardPreview);
    [[nodiscard]] static CompanionContextSnapshot fromNotification(const QString &title,
                                                                   const QString &message,
                                                                   const QString &priority,
                                                                   const QString &sourceAppId = QStringLiteral("vaxil"));
    [[nodiscard]] static QString describeContext(const CompanionContextSnapshot &context);

private:
    [[nodiscard]] static QVariantMap activeWindowMetadata(const QString &normalizedAppId,
                                                          const QString &windowTitle);
    [[nodiscard]] static QString inferredTaskType(const QString &normalizedAppId);
    [[nodiscard]] static QString normalizedAppFamily(const QString &appId);
    [[nodiscard]] static QString inferTopic(const QString &primaryText,
                                            const QString &secondaryText = QString());
    [[nodiscard]] static QString normalizeSegment(QString value);
};
