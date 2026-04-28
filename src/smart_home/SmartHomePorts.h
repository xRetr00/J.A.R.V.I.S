#pragma once

#include <functional>
#include <optional>

#include "smart_home/SmartHomeTypes.h"

class SmartHomeStatePort
{
public:
    virtual ~SmartHomeStatePort() = default;

    virtual void fetchState(std::function<void(const SmartHomeSnapshot &)> callback) = 0;
    virtual SmartHomeSnapshot fetchStateBlocking() = 0;
};

class SmartLightControlPort
{
public:
    virtual ~SmartLightControlPort() = default;

    virtual void executeLightCommand(const SmartLightCommand &command,
                                     std::function<void(const SmartLightCommandResult &)> callback) = 0;
    virtual SmartLightCommandResult executeLightCommandBlocking(const SmartLightCommand &command) = 0;
};

class IdentityPresencePort
{
public:
    virtual ~IdentityPresencePort() = default;

    virtual std::optional<BleIdentitySnapshot> latestIdentityPresence() const = 0;
};
