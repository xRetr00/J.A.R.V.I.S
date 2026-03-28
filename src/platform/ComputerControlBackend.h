#pragma once

#include <QString>
#include <QStringList>

#include "core/ComputerControl.h"

class ComputerControlBackend
{
public:
    virtual ~ComputerControlBackend() = default;

    virtual ComputerControl::ActionResult writeTextFile(const QString &path,
                                                        const QString &content,
                                                        bool overwrite,
                                                        const QString &baseDir) const = 0;
    virtual ComputerControl::ActionResult openUrl(const QString &url) const = 0;
    virtual ComputerControl::ActionResult listApps(const QString &query, int limit) const = 0;
    virtual ComputerControl::ActionResult launchApp(const QString &target, const QStringList &arguments) const = 0;
    virtual ComputerControl::ActionResult setTimer(int durationSeconds,
                                                   const QString &title,
                                                   const QString &message) const = 0;
};
