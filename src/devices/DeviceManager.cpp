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

bool DeviceManager::canExecuteTarget(const QString &target) const
{
    const QString normalized = target.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    for (auto *device : m_devices) {
        if (device->deviceId().compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

QString DeviceManager::execute(const CommandEnvelope &command)
{
    for (auto *device : m_devices) {
        if (device->deviceId().compare(command.target, Qt::CaseInsensitive) == 0) {
            return device->execute(command);
        }
    }

    return QStringLiteral("No device matched the requested target.");
}
