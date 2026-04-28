#include "smart_home/LocalSmartHomeIntentRouter.h"

#include <QtTest/QtTest>

class LocalSmartHomeIntentRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void simpleLightCommandsBypassBackend();
    void statusQuestionsBypassBackend();
    void complexSmartHomeRequestsRemainForBackend();
};

void LocalSmartHomeIntentRouterTests::simpleLightCommandsBypassBackend()
{
    const std::optional<LocalSmartHomeIntent> on = LocalSmartHomeIntentRouter::match(QStringLiteral("turn on the light"));
    QVERIFY(on.has_value());
    QCOMPARE(on->action, QStringLiteral("turn_light_on"));
    QVERIFY(on->backendBypassed);

    const std::optional<LocalSmartHomeIntent> brightness =
        LocalSmartHomeIntentRouter::match(QStringLiteral("set light brightness to 30%"));
    QVERIFY(brightness.has_value());
    QCOMPARE(brightness->action, QStringLiteral("set_light_brightness"));
    QCOMPARE(brightness->brightnessPercent, 30);
    QVERIFY(brightness->backendBypassed);
}

void LocalSmartHomeIntentRouterTests::statusQuestionsBypassBackend()
{
    const std::optional<LocalSmartHomeIntent> room =
        LocalSmartHomeIntentRouter::match(QStringLiteral("is there anyone in the room?"));
    QVERIFY(room.has_value());
    QCOMPARE(room->action, QStringLiteral("get_room_status"));
    QVERIFY(room->backendBypassed);

    const std::optional<LocalSmartHomeIntent> phone =
        LocalSmartHomeIntentRouter::match(QStringLiteral("is my phone detected?"));
    QVERIFY(phone.has_value());
    QCOMPARE(phone->action, QStringLiteral("get_room_status"));
    QVERIFY(phone->backendBypassed);
}

void LocalSmartHomeIntentRouterTests::complexSmartHomeRequestsRemainForBackend()
{
    QVERIFY(!LocalSmartHomeIntentRouter::match(QStringLiteral("set a relaxing scene for movie night")).has_value());
    QVERIFY(!LocalSmartHomeIntentRouter::match(QStringLiteral("if someone enters while I'm away notify me and dim lights")).has_value());
}

QTEST_APPLESS_MAIN(LocalSmartHomeIntentRouterTests)
#include "LocalSmartHomeIntentRouterTests.moc"
