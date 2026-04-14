#pragma once

#include <QString>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class DesktopContextSelectionBuilder
{
public:
    [[nodiscard]] static QString buildSelectionInput(const QString &userInput,
                                                     IntentType intent,
                                                     const QString &desktopSummary,
                                                     const QVariantMap &desktopContext,
                                                     qint64 contextAtMs,
                                                     qint64 nowMs,
                                                     bool privateModeEnabled);

private:
    [[nodiscard]] static QString buildHint(const QString &desktopSummary,
                                           const QVariantMap &desktopContext);
    [[nodiscard]] static bool shouldUseDesktopContext(const QString &userInput,
                                                      IntentType intent,
                                                      const QVariantMap &desktopContext);
};
