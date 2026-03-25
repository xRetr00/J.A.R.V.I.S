#pragma once

#include <array>
#include <memory>
#include <QString>

class SileroVadSession
{
public:
    SileroVadSession();
    ~SileroVadSession();

    bool initialize(const QString &modelPath);
    void reset();
    bool isReady() const;
    float inferProbability(const float *samples, int sampleCount, int sampleRate);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_ready = false;
    QString m_modelPath;
    std::array<float, 512> m_pendingSamples{};
    int m_pendingCount = 0;
    float m_lastProbability = 0.0f;
};
