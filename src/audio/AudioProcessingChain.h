#pragma once

#include <array>
#include <memory>

#include "audio/AudioProcessingTypes.h"

struct Fvad;
class SileroVadSession;

class AudioProcessingChain
{
public:
    AudioProcessingChain();
    ~AudioProcessingChain();

    void initialize(const AudioProcessingConfig &config);
    AudioFrame process(const AudioFrame &in);
    void setFarEnd(const AudioFrame &ttsFrame);

private:
    bool detectVoiceActivity(const AudioFrame &frame);
    float computeRms(const AudioFrame &frame) const;
    bool initializeSileroVad();

    AudioProcessingConfig m_config;
    std::array<float, AudioFrame::kMaxSamples> m_farEndSamples{};
    int m_farEndSampleCount = 0;
    Fvad *m_vad = nullptr;
    std::unique_ptr<SileroVadSession> m_sileroVad;
};
