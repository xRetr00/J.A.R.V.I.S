#pragma once

#include <QString>
#include <QVariantMap>

namespace DesktopWorkMode
{
[[nodiscard]] QString inferFromContext(const QVariantMap &desktopContext);
}
