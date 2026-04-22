#include "tts/TtsVoiceProfilePolicy.h"

#include <cmath>

#include "settings/AppSettings.h"

namespace {
const QList<TtsVoiceProfiles::VoiceProfile> &voiceProfiles()
{
    static const QList<TtsVoiceProfiles::VoiceProfile> profiles = {
        {
            QStringLiteral("fast"),
            QStringLiteral("Fast"),
            1.08,
            1.00,
            0.58,
            0.72,
            0.02,
            QStringLiteral("off")
        },
        {
            QStringLiteral("balanced"),
            QStringLiteral("Balanced"),
            0.95,
            1.00,
            0.67,
            0.80,
            0.06,
            QStringLiteral("light")
        },
        {
            QStringLiteral("natural"),
            QStringLiteral("Natural"),
            0.92,
            1.00,
            0.74,
            0.90,
            0.09,
            QStringLiteral("presence")
        },
        {
            QStringLiteral("warm"),
            QStringLiteral("Warm"),
            0.90,
            0.98,
            0.72,
            0.88,
            0.10,
            QStringLiteral("light")
        },
        {
            QStringLiteral("crisp"),
            QStringLiteral("Crisp"),
            1.00,
            1.02,
            0.60,
            0.76,
            0.05,
            QStringLiteral("presence")
        },
        {
            QStringLiteral("soft"),
            QStringLiteral("Late-night / Soft"),
            0.88,
            0.97,
            0.70,
            0.92,
            0.12,
            QStringLiteral("light")
        }
    };

    return profiles;
}

const TtsVoiceProfiles::VoiceProfile *findProfile(const QString &id)
{
    const QString normalized = id.trimmed().toLower();
    for (const TtsVoiceProfiles::VoiceProfile &profile : voiceProfiles()) {
        if (profile.id == normalized) {
            return &profile;
        }
    }

    return nullptr;
}
} // namespace

namespace TtsVoiceProfiles {

QString normalizeProfileId(const QString &id)
{
    const QString normalized = id.trimmed().toLower();
    if (normalized == QStringLiteral("custom")) {
        return normalized;
    }

    return findProfile(normalized) != nullptr
        ? normalized
        : QStringLiteral("balanced");
}

QStringList profileIds()
{
    QStringList ids;
    for (const VoiceProfile &profile : voiceProfiles()) {
        ids.push_back(profile.id);
    }
    ids.push_back(QStringLiteral("custom"));
    return ids;
}

QStringList profileDisplayNames()
{
    QStringList names;
    for (const VoiceProfile &profile : voiceProfiles()) {
        names.push_back(profile.label);
    }
    names.push_back(QStringLiteral("Custom"));
    return names;
}

bool applyProfile(const QString &id, AppSettings *settings)
{
    if (settings == nullptr) {
        return false;
    }

    const VoiceProfile *profile = findProfile(id);
    if (profile == nullptr) {
        return false;
    }

    settings->setVoiceSpeed(profile->speed);
    settings->setVoicePitch(profile->pitch);
    settings->setPiperNoiseScale(profile->noiseScale);
    settings->setPiperNoiseW(profile->noiseW);
    settings->setPiperSentenceSilence(profile->sentenceSilence);
    settings->setTtsPostProcessMode(profile->postProcessMode);
    settings->setTtsVoiceProfileId(profile->id);
    return true;
}

QString detectClosestProfileId(double speed,
                               double pitch,
                               double noiseScale,
                               double noiseW,
                               double sentenceSilence,
                               const QString &postProcessMode,
                               double tolerance)
{
    const QString normalizedMode = postProcessMode.trimmed().toLower();
    for (const VoiceProfile &profile : voiceProfiles()) {
        if (profile.postProcessMode != normalizedMode) {
            continue;
        }

        if (std::abs(profile.speed - speed) <= tolerance
            && std::abs(profile.pitch - pitch) <= tolerance
            && std::abs(profile.noiseScale - noiseScale) <= tolerance
            && std::abs(profile.noiseW - noiseW) <= tolerance
            && std::abs(profile.sentenceSilence - sentenceSilence) <= tolerance) {
            return profile.id;
        }
    }

    return QStringLiteral("custom");
}

} // namespace TtsVoiceProfiles

