#pragma once

#include <QJsonObject>

#include "core/intent/SmartIntentTypes.h"

class LoggingService;

class RoutingTraceEmitter
{
public:
    QJsonObject buildRouteFinalPayload(const RoutingTrace &trace) const;
    void emitRouteFinal(LoggingService *loggingService, const RoutingTrace &trace) const;
};
