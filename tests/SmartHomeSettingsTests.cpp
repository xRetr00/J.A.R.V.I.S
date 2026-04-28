#include "settings/AppSettings.h"

#include <QtTest/QtTest>

class SmartHomeSettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void smartHomeDefaultsAreSafe();
    void smartHomeSettersClampOperationalRanges();
    void smartHomeTokenEnvVarRejectsSecretValues();
    void smartHomeBleIdentityDefaultsAreSafe();
    void smartHomeBleIdentitySettersNormalizeAndClamp();
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

void SmartHomeSettingsTests::smartHomeTokenEnvVarRejectsSecretValues()
{
    AppSettings settings;

    settings.setSmartHomeHomeAssistantTokenEnvVar(QStringLiteral("not an env var name"));

    QCOMPARE(settings.smartHomeHomeAssistantTokenEnvVar(), QStringLiteral("VAXIL_HOME_ASSISTANT_TOKEN"));
}

void SmartHomeSettingsTests::smartHomeBleIdentityDefaultsAreSafe()
{
    AppSettings settings;

    QVERIFY(settings.smartHomeIdentityMode().isEmpty());
    QVERIFY(settings.smartHomeBleBeaconUuid().isEmpty());
    QCOMPARE(settings.smartHomeBleMissingTimeoutMinutes(), 10);
    QCOMPARE(settings.smartHomeBleScanIntervalMs(), 1000);
    QCOMPARE(settings.smartHomeBleRssiThreshold(), -127);
}

void SmartHomeSettingsTests::smartHomeBleIdentitySettersNormalizeAndClamp()
{
    AppSettings settings;

    settings.setSmartHomeIdentityMode(QStringLiteral(" DESKTOP_BLE_BEACON "));
    settings.setSmartHomeBleBeaconUuid(QStringLiteral("74278BDA-B644-4520-8F0C-720EAF059935"));
    settings.setSmartHomeBleMissingTimeoutMinutes(0);
    settings.setSmartHomeBleScanIntervalMs(100);
    settings.setSmartHomeBleRssiThreshold(-200);

    QCOMPARE(settings.smartHomeIdentityMode(), QStringLiteral("desktop_ble_beacon"));
    QCOMPARE(settings.smartHomeBleBeaconUuid(), QStringLiteral("74278bda-b644-4520-8f0c-720eaf059935"));
    QCOMPARE(settings.smartHomeBleMissingTimeoutMinutes(), 1);
    QCOMPARE(settings.smartHomeBleScanIntervalMs(), 500);
    QCOMPARE(settings.smartHomeBleRssiThreshold(), -127);

    settings.setSmartHomeBleMissingTimeoutMinutes(2000);
    settings.setSmartHomeBleScanIntervalMs(100000);
    settings.setSmartHomeBleRssiThreshold(5);

    QCOMPARE(settings.smartHomeBleMissingTimeoutMinutes(), 1440);
    QCOMPARE(settings.smartHomeBleScanIntervalMs(), 60000);
    QCOMPARE(settings.smartHomeBleRssiThreshold(), 0);
}

QTEST_APPLESS_MAIN(SmartHomeSettingsTests)
#include "SmartHomeSettingsTests.moc"
