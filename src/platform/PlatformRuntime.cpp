#include "platform/PlatformRuntime.h"

QVariantMap platformCapabilitiesToVariantMap(const PlatformCapabilities &capabilities)
{
    QVariantMap map;
    map.insert(QStringLiteral("platformId"), capabilities.platformId);
    map.insert(QStringLiteral("platformLabel"), capabilities.platformLabel);
    map.insert(QStringLiteral("supportsGlobalHotkey"), capabilities.supportsGlobalHotkey);
    map.insert(QStringLiteral("supportsAppListing"), capabilities.supportsAppListing);
    map.insert(QStringLiteral("supportsAppLaunch"), capabilities.supportsAppLaunch);
    map.insert(QStringLiteral("supportsTimerNotification"), capabilities.supportsTimerNotification);
    map.insert(QStringLiteral("supportsAutoToolInstall"), capabilities.supportsAutoToolInstall);
    return map;
}

namespace PlatformRuntime {
namespace {

PlatformCapabilities detectCapabilities()
{
    PlatformCapabilities capabilities;

#if defined(Q_OS_WIN)
    capabilities.platformId = QStringLiteral("windows");
    capabilities.platformLabel = QStringLiteral("Windows");
    capabilities.supportsGlobalHotkey = true;
    capabilities.supportsAppListing = true;
    capabilities.supportsAppLaunch = true;
    capabilities.supportsTimerNotification = true;
    capabilities.supportsAutoToolInstall = true;
#elif defined(Q_OS_LINUX)
    capabilities.platformId = QStringLiteral("linux");
    capabilities.platformLabel = QStringLiteral("Linux");
    capabilities.supportsGlobalHotkey = false;
    capabilities.supportsAppListing = false;
    capabilities.supportsAppLaunch = false;
    capabilities.supportsTimerNotification = false;
    capabilities.supportsAutoToolInstall = true;
#else
    capabilities.platformId = QStringLiteral("unknown");
    capabilities.platformLabel = QStringLiteral("Unknown");
#endif

    return capabilities;
}

}

PlatformCapabilities currentCapabilities()
{
    static const PlatformCapabilities capabilities = detectCapabilities();
    return capabilities;
}

QString platformId()
{
    return currentCapabilities().platformId;
}

QString platformLabel()
{
    return currentCapabilities().platformLabel;
}

bool isWindows()
{
    return currentCapabilities().platformId == QStringLiteral("windows");
}

bool isLinux()
{
    return currentCapabilities().platformId == QStringLiteral("linux");
}

QString executableName(const QString &baseName)
{
    if (baseName.trimmed().isEmpty()) {
        return {};
    }
    return isWindows() ? baseName + QStringLiteral(".exe") : baseName;
}

QString helperExecutableName(const QString &baseName)
{
    return executableName(baseName);
}

QStringList executableFileNames(const QStringList &baseNames)
{
    QStringList names;
    for (const QString &baseName : baseNames) {
        const QString trimmed = baseName.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.contains(QChar::fromLatin1('.'))) {
            names.push_back(trimmed);
        } else {
            names.push_back(executableName(trimmed));
        }
    }
    names.removeDuplicates();
    return names;
}

QStringList whisperExecutableNames()
{
    return isWindows()
        ? QStringList{
            QStringLiteral("whisper-cli.exe"),
            QStringLiteral("main.exe"),
            QStringLiteral("whisper.exe")
        }
        : QStringList{
            QStringLiteral("whisper-cli"),
            QStringLiteral("whisper"),
            QStringLiteral("main")
        };
}

QStringList piperExecutableNames()
{
    return executableFileNames({QStringLiteral("piper")});
}

QStringList ffmpegExecutableNames()
{
    return executableFileNames({QStringLiteral("ffmpeg")});
}

QStringList sharedLibraryPatterns(const QString &baseName)
{
    const QString trimmed = baseName.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (isWindows()) {
        return {trimmed + QStringLiteral(".dll")};
    }

    return {
        QStringLiteral("lib%1.so").arg(trimmed),
        QStringLiteral("lib%1.so.*").arg(trimmed)
    };
}

}
