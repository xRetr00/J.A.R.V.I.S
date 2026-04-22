#pragma once

#include "core/intent/SmartIntentTypes.h"

struct RoutingTraceAnalyzerSummary
{
    int total = 0;
    int misroutes = 0;
    int overrideCount = 0;
    int highAmbiguityCount = 0;
    int fallbackCount = 0;
    QStringList topProblemPatterns;
};

class RoutingTraceAnalyzer
{
public:
    RoutingTraceAnalyzerSummary summarize(const QList<RoutingTrace> &traces) const;
};
