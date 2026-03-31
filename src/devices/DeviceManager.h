#pragma once

#include <QObject>

#include "devices/Device.h"

class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);

    void registerDefaults();
    bool canExecuteTarget(const QString &target) const;
    QString execute(const CommandEnvelope &command);

private:
    QList<Device *> m_devices;
};
