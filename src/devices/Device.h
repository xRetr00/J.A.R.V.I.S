#pragma once

#include <QString>
#include <QStringList>

#include "core/AssistantTypes.h"

class Device
{
public:
    virtual ~Device() = default;

    virtual QString deviceId() const = 0;
    virtual QStringList capabilities() const = 0;
    virtual bool turnOn() = 0;
    virtual bool turnOff() = 0;
    virtual QString execute(const CommandEnvelope &command) = 0;
};
