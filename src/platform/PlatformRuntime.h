#pragma once

#include <QString>
#include <QStringList>

#include "platform/PlatformCapabilities.h"

namespace PlatformRuntime {

PlatformCapabilities currentCapabilities();
QString platformId();
QString platformLabel();
bool isWindows();
bool isLinux();
QString executableName(const QString &baseName);
QString helperExecutableName(const QString &baseName);
QStringList executableFileNames(const QStringList &baseNames);
QStringList whisperExecutableNames();
QStringList piperExecutableNames();
QStringList ffmpegExecutableNames();
QStringList sharedLibraryPatterns(const QString &baseName);

}
