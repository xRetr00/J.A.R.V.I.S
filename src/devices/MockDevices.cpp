#include "devices/MockDevices.h"

MockDeviceBase::MockDeviceBase(QString id, QStringList capabilities, QObject *parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_capabilities(std::move(capabilities))
{
}

QString MockDeviceBase::deviceId() const
{
    return m_id;
}

QStringList MockDeviceBase::capabilities() const
{
    return m_capabilities;
}

bool MockDeviceBase::turnOn()
{
    m_on = true;
    return true;
}

bool MockDeviceBase::turnOff()
{
    m_on = false;
    return true;
}

QString MockDeviceBase::execute(const CommandEnvelope &command)
{
    if (command.action == QStringLiteral("turn_on")) {
        turnOn();
        return QStringLiteral("%1 turned on.").arg(m_id);
    }

    if (command.action == QStringLiteral("turn_off")) {
        turnOff();
        return QStringLiteral("%1 turned off.").arg(m_id);
    }

    return QStringLiteral("%1 received %2.").arg(m_id, command.action);
}

MockLightDevice::MockLightDevice(QObject *parent)
    : MockDeviceBase(QStringLiteral("desk_light"), {QStringLiteral("power")}, parent)
{
}

MockSpeakerDevice::MockSpeakerDevice(QObject *parent)
    : MockDeviceBase(QStringLiteral("speaker"), {QStringLiteral("power"), QStringLiteral("volume")}, parent)
{
}
