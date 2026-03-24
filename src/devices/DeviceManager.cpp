#include "devices/DeviceManager.h"

#include "devices/MockDevices.h"

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
}

void DeviceManager::registerDefaults()
{
    m_devices = {
        new MockLightDevice(this),
        new MockSpeakerDevice(this)
    };
}

QString DeviceManager::execute(const CommandEnvelope &command)
{
    for (auto *device : m_devices) {
        if (device->deviceId() == command.target) {
            return device->execute(command);
        }
    }

    return QStringLiteral("No device matched the requested target.");
}
