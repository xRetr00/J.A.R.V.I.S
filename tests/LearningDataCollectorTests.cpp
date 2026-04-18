#include <QtTest>

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>

#include "learning_data/LearningDataCollector.h"
#include "learning_data/LearningDataStorage.h"
#include "learning_data/LearningDataTypes.h"

namespace {

QByteArray buildPcmClip(int durationMs, int sampleRate = 16000)
{
    const int sampleCount = std::max(1, (durationMs * sampleRate) / 1000);
    QByteArray pcm;
    pcm.reserve(sampleCount * static_cast<int>(sizeof(qint16)));

    for (int i = 0; i < sampleCount; ++i) {
        const double phase = (static_cast<double>(i % 64) / 64.0) * 6.283185307179586;
        const qint16 value = static_cast<qint16>(std::sin(phase) * 12000.0);
        pcm.append(reinterpret_cast<const char *>(&value), static_cast<int>(sizeof(value)));
    }
    return pcm;
}

QList<QJsonObject> readJsonl(const QString &absolutePath)
{
    QList<QJsonObject> rows;
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return rows;
    }

    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            rows.push_back(doc.object());
        }
    }

    return rows;
}

LearningData::LearningDataSettingsSnapshot enabledWakeSettings(const QString &root)
{
    LearningData::LearningDataSettingsSnapshot settings;
    settings.enabled = true;
    settings.audioCollectionEnabled = true;
    settings.wakeWordCollectionEnabled = true;
    settings.wakeWordPositiveCollectionEnabled = true;
    settings.wakeWordNegativeCollectionEnabled = true;
    settings.wakeWordHardNegativeCollectionEnabled = true;
    settings.transcriptCollectionEnabled = true;
    settings.toolLoggingEnabled = true;
    settings.behaviorLoggingEnabled = true;
    settings.memoryLoggingEnabled = true;
    settings.maxAudioStorageGb = 4.0;
    settings.maxWakeWordStorageGb = 2.0;
    settings.maxDaysToKeepAudio = 30;
    settings.maxDaysToKeepWakeWordAudio = 30;
    settings.maxDaysToKeepStructuredLogs = 90;
    settings.allowPreparedDatasetExport = true;
    settings.rootDirectory = root;
    return settings;
}

} // namespace

class LearningDataCollectorTests : public QObject
{
    Q_OBJECT

private slots:
    void toolDecisionJsonRoundTripsSchema();
    void wakeWordEventJsonRoundTripsSchema();
    void wakeWordStorageWritesRoleBucketsAndIndices();
    void wakeWordExportManifestIncludesLabels();
    void wakeWordRetentionRemovesExpiredFiles();
    void wakeWordFileNamingIsCollisionSafe();
    void wakeWordCollectorDisabledSettingsNoOp();
};

void LearningDataCollectorTests::toolDecisionJsonRoundTripsSchema()
{
    LearningData::ToolDecisionEvent input;
    input.sessionId = QStringLiteral("session-1");
    input.turnId = QStringLiteral("1");
    input.eventId = QStringLiteral("tool_decision_1");
    input.timestamp = LearningData::toIsoUtcNow();
    input.userInputText = QStringLiteral("open calculator");
    input.inputMode = QStringLiteral("voice");
    input.availableTools = {QStringLiteral("calculator"), QStringLiteral("notepad")};
    input.selectedTool = QStringLiteral("calculator");
    input.candidateToolsWithScores = {
        LearningData::ToolCandidateScore{QStringLiteral("calculator"), 0.91},
        LearningData::ToolCandidateScore{QStringLiteral("notepad"), 0.32}
    };
    input.decisionSource = QStringLiteral("heuristic");
    input.expectedConfirmationLevel = QStringLiteral("none");

    const QJsonObject json = LearningData::toJson(input);
    QCOMPARE(json.value(QStringLiteral("schema_version")).toString(), LearningData::kSchemaVersion);

    const LearningData::ToolDecisionEvent output = LearningData::toolDecisionEventFromJson(json);
    QCOMPARE(output.schemaVersion, LearningData::kSchemaVersion);
    QCOMPARE(output.sessionId, input.sessionId);
    QCOMPARE(output.turnId, input.turnId);
    QCOMPARE(output.eventId, input.eventId);
    QCOMPARE(output.selectedTool, input.selectedTool);
    QCOMPARE(output.availableTools.size(), input.availableTools.size());
    QCOMPARE(output.candidateToolsWithScores.size(), input.candidateToolsWithScores.size());
}

void LearningDataCollectorTests::wakeWordEventJsonRoundTripsSchema()
{
    LearningData::WakeWordEvent input;
    input.eventId = QStringLiteral("wakeword_1");
    input.sessionId = QStringLiteral("session-ww");
    input.turnId = QStringLiteral("turn-1");
    input.timestamp = LearningData::toIsoUtcNow();
    input.filePath = QStringLiteral("wakeword/positive/2026/04/session_x/wwpos.wav");
    input.clipRole = LearningData::WakeWordClipRole::Positive;
    input.labelStatus = LearningData::WakeWordLabelStatus::AutoLabeled;
    input.wakeEngine = QStringLiteral("sherpa-onnx");
    input.keywordText = QStringLiteral("Hey Vaxil");
    input.detected = true;
    input.detectionScore = 0.93;
    input.detectionScoreAvailable = true;
    input.threshold = 0.80;
    input.thresholdAvailable = true;
    input.durationMs = 1350;
    input.sampleRate = 16000;
    input.channels = 1;
    input.captureSource = QStringLiteral("default_mic");
    input.collectionReason = QStringLiteral("wakeword_detected_trigger");
    input.wasUsedToStartSession = true;
    input.cameFromFalseTrigger = false;
    input.cameFromMissedTriggerRecovery = false;
    input.notes = QStringLiteral("test");
    input.fileHashSha256 = QStringLiteral("abc123");

    const QJsonObject json = LearningData::toJson(input);
    QCOMPARE(json.value(QStringLiteral("schema_version")).toString(), LearningData::kSchemaVersion);
    QCOMPARE(json.value(QStringLiteral("clip_role")).toString(), QStringLiteral("positive"));

    const LearningData::WakeWordEvent output = LearningData::wakeWordEventFromJson(json);
    QCOMPARE(output.schemaVersion, LearningData::kSchemaVersion);
    QCOMPARE(output.eventId, input.eventId);
    QCOMPARE(output.sessionId, input.sessionId);
    QCOMPARE(output.clipRole, LearningData::WakeWordClipRole::Positive);
    QCOMPARE(output.labelStatus, LearningData::WakeWordLabelStatus::AutoLabeled);
    QCOMPARE(output.detectionScoreAvailable, true);
    QCOMPARE(output.thresholdAvailable, true);
    QCOMPARE(output.durationMs, input.durationMs);
}

void LearningDataCollectorTests::wakeWordStorageWritesRoleBucketsAndIndices()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    LearningData::LearningDataStorage storage(tempDir.path(), nullptr);
    LearningData::LearningDataSettingsSnapshot settings = enabledWakeSettings(tempDir.path());
    QVERIFY(storage.initialize(settings));

    const QByteArray clip = buildPcmClip(1200);

    LearningData::WakeWordEvent positive;
    positive.eventId = QStringLiteral("ww_pos_1");
    positive.sessionId = QStringLiteral("session-a");
    positive.timestamp = LearningData::toIsoUtcNow();
    positive.clipRole = LearningData::WakeWordClipRole::Positive;
    positive.labelStatus = LearningData::WakeWordLabelStatus::AutoLabeled;
    positive.wakeEngine = QStringLiteral("sherpa-onnx");
    positive.keywordText = QStringLiteral("Hey Vaxil");
    positive.detected = true;
    positive.sampleRate = 16000;
    positive.channels = 1;
    positive.collectionReason = QStringLiteral("wakeword_detected_trigger");
    QVERIFY(storage.writeWakeWordEvent(positive, clip));

    LearningData::WakeWordEvent negative = positive;
    negative.eventId = QStringLiteral("ww_neg_1");
    negative.clipRole = LearningData::WakeWordClipRole::Negative;
    negative.detected = false;
    negative.collectionReason = QStringLiteral("non_trigger_speech_sample");
    QVERIFY(storage.writeWakeWordEvent(negative, clip));

    LearningData::WakeWordEvent hardNegative = positive;
    hardNegative.eventId = QStringLiteral("ww_hneg_1");
    hardNegative.clipRole = LearningData::WakeWordClipRole::FalseAccept;
    hardNegative.collectionReason = QStringLiteral("false_trigger_no_speech");
    hardNegative.cameFromFalseTrigger = true;
    QVERIFY(storage.writeWakeWordEvent(hardNegative, clip));

    const QString eventsPath = QDir(tempDir.path()).filePath(QStringLiteral("index/wakeword_events.jsonl"));
    const QString manifestPath = QDir(tempDir.path()).filePath(QStringLiteral("index/wakeword_manifest.jsonl"));
    const QList<QJsonObject> rows = readJsonl(eventsPath);
    const QList<QJsonObject> manifestRows = readJsonl(manifestPath);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(manifestRows.size(), 3);

    bool hasPositivePath = false;
    bool hasNegativePath = false;
    bool hasHardNegativePath = false;
    for (const QJsonObject &row : rows) {
        const QString filePath = row.value(QStringLiteral("file_path")).toString();
        hasPositivePath = hasPositivePath || filePath.contains(QStringLiteral("wakeword/positive/"));
        hasNegativePath = hasNegativePath || filePath.contains(QStringLiteral("wakeword/negative/"));
        hasHardNegativePath = hasHardNegativePath || filePath.contains(QStringLiteral("wakeword/hard_negative/"));
    }

    QVERIFY(hasPositivePath);
    QVERIFY(hasNegativePath);
    QVERIFY(hasHardNegativePath);
}

void LearningDataCollectorTests::wakeWordExportManifestIncludesLabels()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    LearningData::LearningDataStorage storage(tempDir.path(), nullptr);
    LearningData::LearningDataSettingsSnapshot settings = enabledWakeSettings(tempDir.path());
    settings.allowPreparedDatasetExport = true;
    QVERIFY(storage.initialize(settings));

    const QByteArray clip = buildPcmClip(900);
    for (const LearningData::WakeWordClipRole role : {
             LearningData::WakeWordClipRole::Positive,
             LearningData::WakeWordClipRole::Negative,
             LearningData::WakeWordClipRole::FalseAccept }) {
        LearningData::WakeWordEvent event;
        event.eventId = LearningData::LearningDataCollector::createEventId(QStringLiteral("wakeword_test"));
        event.sessionId = QStringLiteral("session-export");
        event.timestamp = LearningData::toIsoUtcNow();
        event.clipRole = role;
        event.labelStatus = LearningData::WakeWordLabelStatus::Assumed;
        event.wakeEngine = QStringLiteral("sherpa-onnx");
        event.keywordText = QStringLiteral("Hey Vaxil");
        event.sampleRate = 16000;
        event.channels = 1;
        event.collectionReason = QStringLiteral("export_test");
        QVERIFY(storage.writeWakeWordEvent(event, clip));
    }

    QVERIFY(storage.exportPreparedManifests(settings));

    QDir exportsDir(QDir(tempDir.path()).filePath(QStringLiteral("exports")));
    const QStringList dirs = exportsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    QVERIFY(!dirs.isEmpty());

    const QString wakeManifestPath = exportsDir.filePath(dirs.first() + QStringLiteral("/wakeword_manifest.jsonl"));
    const QList<QJsonObject> rows = readJsonl(wakeManifestPath);
    QCOMPARE(rows.size(), 3);

    QSet<QString> labels;
    for (const QJsonObject &row : rows) {
        labels.insert(row.value(QStringLiteral("label")).toString());
    }

    QVERIFY(labels.contains(QStringLiteral("positive")));
    QVERIFY(labels.contains(QStringLiteral("negative")));
    QVERIFY(labels.contains(QStringLiteral("false_accept")));
}

void LearningDataCollectorTests::wakeWordRetentionRemovesExpiredFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    LearningData::LearningDataStorage storage(tempDir.path(), nullptr);
    LearningData::LearningDataSettingsSnapshot settings = enabledWakeSettings(tempDir.path());
    settings.maxDaysToKeepWakeWordAudio = 1;
    settings.maxWakeWordStorageGb = 8.0;
    QVERIFY(storage.initialize(settings));

    LearningData::WakeWordEvent event;
    event.eventId = QStringLiteral("ww_old_1");
    event.sessionId = QStringLiteral("session-old");
    event.timestamp = LearningData::toIsoUtcNow();
    event.clipRole = LearningData::WakeWordClipRole::Positive;
    event.labelStatus = LearningData::WakeWordLabelStatus::Assumed;
    event.sampleRate = 16000;
    event.channels = 1;
    QVERIFY(storage.writeWakeWordEvent(event, buildPcmClip(800)));

    const QList<QJsonObject> rows = readJsonl(QDir(tempDir.path()).filePath(QStringLiteral("index/wakeword_events.jsonl")));
    QVERIFY(!rows.isEmpty());
    const QString relativePath = rows.first().value(QStringLiteral("file_path")).toString();
    const QString absolutePath = QDir(tempDir.path()).filePath(relativePath);
    QVERIFY(QFileInfo::exists(absolutePath));

    QFile clipFile(absolutePath);
    QVERIFY(clipFile.open(QIODevice::ReadOnly));
    QVERIFY(clipFile.setFileTime(QDateTime::currentDateTimeUtc().addDays(-10), QFileDevice::FileModificationTime));
    clipFile.close();

    QVERIFY(storage.runRetention(settings));
    QVERIFY(!QFileInfo::exists(absolutePath));
}

void LearningDataCollectorTests::wakeWordFileNamingIsCollisionSafe()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    LearningData::LearningDataStorage storage(tempDir.path(), nullptr);
    LearningData::LearningDataSettingsSnapshot settings = enabledWakeSettings(tempDir.path());
    QVERIFY(storage.initialize(settings));

    LearningData::WakeWordEvent event;
    event.eventId = QStringLiteral("same_event_id");
    event.sessionId = QStringLiteral("session-collision");
    event.timestamp = QStringLiteral("2026-04-18T10:11:12.333Z");
    event.clipRole = LearningData::WakeWordClipRole::Positive;
    event.labelStatus = LearningData::WakeWordLabelStatus::Assumed;
    event.sampleRate = 16000;
    event.channels = 1;

    const QByteArray clip = buildPcmClip(600);
    QVERIFY(storage.writeWakeWordEvent(event, clip));
    QVERIFY(storage.writeWakeWordEvent(event, clip));

    const QList<QJsonObject> rows = readJsonl(QDir(tempDir.path()).filePath(QStringLiteral("index/wakeword_events.jsonl")));
    QCOMPARE(rows.size(), 2);
    const QString pathA = rows.at(0).value(QStringLiteral("file_path")).toString();
    const QString pathB = rows.at(1).value(QStringLiteral("file_path")).toString();
    QVERIFY(pathA != pathB);
    QVERIFY(QFileInfo::exists(QDir(tempDir.path()).filePath(pathA)));
    QVERIFY(QFileInfo::exists(QDir(tempDir.path()).filePath(pathB)));
}

void LearningDataCollectorTests::wakeWordCollectorDisabledSettingsNoOp()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    LearningData::LearningDataCollector collector(nullptr, nullptr, tempDir.path());
    LearningData::WakeWordEvent event;
    event.eventId = QStringLiteral("noop_event");
    event.sessionId = QStringLiteral("session-noop");
    event.timestamp = LearningData::toIsoUtcNow();
    event.clipRole = LearningData::WakeWordClipRole::Positive;
    event.labelStatus = LearningData::WakeWordLabelStatus::AutoLabeled;
    event.sampleRate = 16000;
    event.channels = 1;

    collector.recordWakeWordEvent(event, buildPcmClip(400));
    collector.waitForIdle();

    const QString wakeEventsPath = QDir(tempDir.path()).filePath(QStringLiteral("index/wakeword_events.jsonl"));
    QVERIFY(!QFileInfo::exists(wakeEventsPath));
}

QTEST_APPLESS_MAIN(LearningDataCollectorTests)
#include "LearningDataCollectorTests.moc"
