#include "smart_home/HomeAssistantSmartHomeAdapter.h"

#include <QtTest/QtTest>

class HomeAssistantSmartHomeAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    void deviceTrackerHomeMapsPresent();
    void deviceTrackerNotHomeMapsAbsent();
    void deviceTrackerUnavailableMapsStale();
    void namedZoneMapsAbsentForNow();
};

void HomeAssistantSmartHomeAdapterTests::deviceTrackerHomeMapsPresent()
{
    const BleIdentitySnapshot snapshot = HomeAssistantSmartHomeAdapter::parseIdentityState(
        QJsonObject{{QStringLiteral("state"), QStringLiteral("home")}},
        QStringLiteral("device_tracker.my_iphone"),
        1000);

    QVERIFY(snapshot.available);
    QVERIFY(snapshot.present);
    QVERIFY(!snapshot.stale);
    QCOMPARE(snapshot.source, QStringLiteral("home_assistant_device_tracker"));
    QCOMPARE(snapshot.rawState, QStringLiteral("home"));
}

void HomeAssistantSmartHomeAdapterTests::deviceTrackerNotHomeMapsAbsent()
{
    const BleIdentitySnapshot snapshot = HomeAssistantSmartHomeAdapter::parseIdentityState(
        QJsonObject{{QStringLiteral("state"), QStringLiteral("not_home")}},
        QStringLiteral("device_tracker.my_iphone"),
        1000);

    QVERIFY(snapshot.available);
    QVERIFY(!snapshot.present);
    QVERIFY(!snapshot.stale);
}

void HomeAssistantSmartHomeAdapterTests::deviceTrackerUnavailableMapsStale()
{
    const BleIdentitySnapshot snapshot = HomeAssistantSmartHomeAdapter::parseIdentityState(
        QJsonObject{{QStringLiteral("state"), QStringLiteral("unavailable")}},
        QStringLiteral("device_tracker.my_iphone"),
        1000);

    QVERIFY(!snapshot.available);
    QVERIFY(!snapshot.present);
    QVERIFY(snapshot.stale);
}

void HomeAssistantSmartHomeAdapterTests::namedZoneMapsAbsentForNow()
{
    const BleIdentitySnapshot snapshot = HomeAssistantSmartHomeAdapter::parseIdentityState(
        QJsonObject{{QStringLiteral("state"), QStringLiteral("work")}},
        QStringLiteral("device_tracker.my_iphone"),
        1000);

    QVERIFY(snapshot.available);
    QVERIFY(!snapshot.present);
    QVERIFY(!snapshot.stale);
}

QTEST_APPLESS_MAIN(HomeAssistantSmartHomeAdapterTests)
#include "HomeAssistantSmartHomeAdapterTests.moc"
