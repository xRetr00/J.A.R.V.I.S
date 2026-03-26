#pragma once

#include <QString>
#include <QStringList>

namespace ComputerControl {

struct ActionResult
{
    bool success = false;
    QString summary;
    QString detail;
    QStringList lines;
    QString resolvedPath;
};

ActionResult writeTextFile(const QString &path,
                           const QString &content,
                           bool overwrite = false,
                           const QString &baseDir = QString());
ActionResult openUrl(const QString &url);
ActionResult listApps(const QString &query = QString(), int limit = 20);
ActionResult launchApp(const QString &target, const QStringList &arguments = {});
ActionResult setTimer(int durationSeconds,
                      const QString &title = QString(),
                      const QString &message = QString());

}
