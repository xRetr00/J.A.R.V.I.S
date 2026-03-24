#pragma once

#include <QObject>

#include "devices/Device.h"

class MockDeviceBase : public QObject, public Device
{
    Q_OBJECT

public:
    explicit MockDeviceBase(QString id, QStringList capabilities, QObject *parent = nullptr);

    QString deviceId() const override;
    QStringList capabilities() const override;
    bool turnOn() override;
    bool turnOff() override;
    QString execute(const CommandEnvelope &command) override;

protected:
    QString m_id;
    QStringList m_capabilities;
    bool m_on = false;
};

class MockLightDevice : public MockDeviceBase
{
    Q_OBJECT

public:
    explicit MockLightDevice(QObject *parent = nullptr);
};

class MockSpeakerDevice : public MockDeviceBase
{
    Q_OBJECT

public:
    explicit MockSpeakerDevice(QObject *parent = nullptr);
};
