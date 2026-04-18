#pragma once

#include <QVariantMap>

#include "core/AssistantTypes.h"

class DesktopActionContextPolicy
{
public:
    [[nodiscard]] static TrustDecision applyToTrust(const QVariantMap &desktopContext,
                                                    const ToolPlan &plan,
                                                    const TrustDecision &trust);
    [[nodiscard]] static bool shouldQuietProgress(const QVariantMap &desktopContext,
                                                  const TrustDecision &trust);
};
