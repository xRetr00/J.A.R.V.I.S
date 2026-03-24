#pragma once

#include <QObject>

#include "devices/Device.h"

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);

    void registerDefaults();
    QString execute(const CommandEnvelope &command) const;

private:
    QList<Device *> m_devices;
};
