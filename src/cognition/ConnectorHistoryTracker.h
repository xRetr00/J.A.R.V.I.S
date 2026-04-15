#pragma once

#include <QHash>
#include <QString>
#include <QVariantMap>

class ConnectorHistoryTracker
{
public:
    struct Entry {
        QString sourceKind;
        QString connectorKind;
        qint64 firstSeenAtMs = 0;
        qint64 lastSeenAtMs = 0;
        qint64 lastPresentedAtMs = 0;
        int seenCount = 0;
        int presentedCount = 0;
    };

    explicit ConnectorHistoryTracker(class MemoryStore *memoryStore = nullptr,
                                     const QString &storagePath = QString());

    void recordSeen(const QString &sourceKind,
                    const QString &connectorKind,
                    const QString &historyKey,
                    qint64 nowMs);
    void recordPresented(const QString &historyKey, qint64 nowMs);
    [[nodiscard]] QVariantMap buildMetadata(const QString &sourceKind,
                                            const QString &connectorKind,
                                            const QString &historyKey,
                                            qint64 nowMs);

private:
    void loadIfNeeded();
    void persist() const;
    void prune(qint64 nowMs);

    class MemoryStore *m_memoryStore = nullptr;
    QString m_storagePath;
    bool m_loaded = false;
    QHash<QString, Entry> m_entriesByKey;
};
