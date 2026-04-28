#include "settings/AppSettings.h"

#include <QtTest/QtTest>

class SmartHomeSettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void smartHomeDefaultsAreSafe();
    void smartHomeSettersClampOperationalRanges();
};

void SmartHomeSettingsTests::smartHomeDefaultsAreSafe()
{
    AppSettings settings;

    QVERIFY(!settings.smartHomeEnabled());
    QCOMPARE(settings.smartHomeProvider(), QStringLiteral("home_assistant"));
    QCOMPARE(settings.smartHomeHomeAssistantTokenEnvVar(), QStringLiteral("VAXIL_HOME_ASSISTANT_TOKEN"));
    QVERIFY(!settings.smartHomeSensorOnlyWelcomeEnabled());
    QCOMPARE(settings.smartHomeWelcomeCooldownMinutes(), 30);
    QCOMPARE(settings.smartHomeRoomAbsenceGraceMinutes(), 6);
}

void SmartHomeSettingsTests::smartHomeSettersClampOperationalRanges()
{
    AppSettings settings;

    settings.setSmartHomePollIntervalMs(50);
    settings.setSmartHomeRequestTimeoutMs(50);
    settings.setSmartHomeWelcomeCooldownMinutes(-1);
    settings.setSmartHomeRoomAbsenceGraceMinutes(100);

    QCOMPARE(settings.smartHomePollIntervalMs(), 1000);
    QCOMPARE(settings.smartHomeRequestTimeoutMs(), 500);
    QCOMPARE(settings.smartHomeWelcomeCooldownMinutes(), 0);
    QCOMPARE(settings.smartHomeRoomAbsenceGraceMinutes(), 30);
}

QTEST_APPLESS_MAIN(SmartHomeSettingsTests)
#include "SmartHomeSettingsTests.moc"
