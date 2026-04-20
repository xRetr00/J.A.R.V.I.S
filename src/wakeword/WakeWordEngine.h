#pragma once

#include <QObject>

#include "audio/AudioProcessingTypes.h"

class WakeWordEngine : public QObject
{
    Q_OBJECT

public:
    explicit WakeWordEngine(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~WakeWordEngine() override = default;

    virtual bool start(
        const QString &enginePath,
        const QString &modelPath,
        float threshold,
        int cooldownMs,
        const QString &preferredDeviceId = {}) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual bool isActive() const = 0;
    virtual bool isPaused() const = 0;
    virtual bool usesExternalAudioInput() const { return false; }
    virtual void processAudioFrame(const AudioFrame &frame) { Q_UNUSED(frame); }

signals:
    void engineReady();
    void probabilityUpdated(float probability);
    void wakeWordDetected();
    void wakeKeywordDetected(const QString &keyword);
    void errorOccurred(const QString &message);
};
