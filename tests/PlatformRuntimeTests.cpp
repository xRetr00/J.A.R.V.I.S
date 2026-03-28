#include <QtTest>

#include "platform/PlatformRuntime.h"

class PlatformRuntimeTests : public QObject
{
    Q_OBJECT

private slots:
    void reportsExpectedCapabilities();
    void usesExpectedExecutableNames();
    void usesExpectedSharedLibraryPatterns();
};

void PlatformRuntimeTests::reportsExpectedCapabilities()
{
    const PlatformCapabilities capabilities = PlatformRuntime::currentCapabilities();

#if defined(Q_OS_WIN)
    QCOMPARE(capabilities.platformId, QStringLiteral("windows"));
    QVERIFY(capabilities.supportsGlobalHotkey);
    QVERIFY(capabilities.supportsAppListing);
    QVERIFY(capabilities.supportsAppLaunch);
    QVERIFY(capabilities.supportsTimerNotification);
    QVERIFY(capabilities.supportsAutoToolInstall);
#elif defined(Q_OS_LINUX)
    QCOMPARE(capabilities.platformId, QStringLiteral("linux"));
    QVERIFY(!capabilities.supportsGlobalHotkey);
    QVERIFY(!capabilities.supportsAppListing);
    QVERIFY(!capabilities.supportsAppLaunch);
    QVERIFY(!capabilities.supportsTimerNotification);
    QVERIFY(capabilities.supportsAutoToolInstall);
#else
    QVERIFY(!capabilities.platformId.isEmpty());
#endif
}

void PlatformRuntimeTests::usesExpectedExecutableNames()
{
#if defined(Q_OS_WIN)
    QCOMPARE(PlatformRuntime::helperExecutableName(QStringLiteral("jarvis_sherpa_wake_helper")),
             QStringLiteral("jarvis_sherpa_wake_helper.exe"));
    QVERIFY(PlatformRuntime::whisperExecutableNames().contains(QStringLiteral("whisper-cli.exe")));
    QVERIFY(PlatformRuntime::piperExecutableNames().contains(QStringLiteral("piper.exe")));
    QVERIFY(PlatformRuntime::ffmpegExecutableNames().contains(QStringLiteral("ffmpeg.exe")));
#elif defined(Q_OS_LINUX)
    QCOMPARE(PlatformRuntime::helperExecutableName(QStringLiteral("jarvis_sherpa_wake_helper")),
             QStringLiteral("jarvis_sherpa_wake_helper"));
    QVERIFY(PlatformRuntime::whisperExecutableNames().contains(QStringLiteral("whisper-cli")));
    QVERIFY(PlatformRuntime::piperExecutableNames().contains(QStringLiteral("piper")));
    QVERIFY(PlatformRuntime::ffmpegExecutableNames().contains(QStringLiteral("ffmpeg")));
#endif
}

void PlatformRuntimeTests::usesExpectedSharedLibraryPatterns()
{
    const QStringList patterns = PlatformRuntime::sharedLibraryPatterns(QStringLiteral("onnxruntime"));
#if defined(Q_OS_WIN)
    QCOMPARE(patterns, QStringList{QStringLiteral("onnxruntime.dll")});
#elif defined(Q_OS_LINUX)
    QVERIFY(patterns.contains(QStringLiteral("libonnxruntime.so")));
    QVERIFY(patterns.contains(QStringLiteral("libonnxruntime.so.*")));
#endif
}

QTEST_APPLESS_MAIN(PlatformRuntimeTests)
#include "PlatformRuntimeTests.moc"
