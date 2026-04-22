#pragma once

#include <QString>
#include <QStringList>
#include <QList>

class AppSettings;

namespace TtsVoiceProfiles {

struct VoiceProfile
{
    QString id;
    QString label;
    double speed = 0.95;
    double pitch = 1.00;
    double noiseScale = 0.67;
    double noiseW = 0.80;
    double sentenceSilence = 0.06;
    QString postProcessMode = QStringLiteral("light");
};

QString normalizeProfileId(const QString &id);
QStringList profileIds();
QStringList profileDisplayNames();
bool applyProfile(const QString &id, AppSettings *settings);
QString detectClosestProfileId(double speed,
                               double pitch,
                               double noiseScale,
                               double noiseW,
                               double sentenceSilence,
                               const QString &postProcessMode,
                               double tolerance = 0.02);

} // namespace TtsVoiceProfiles
