#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include "core/AssistantTypes.h"

class MemoryManager
{
public:
    explicit MemoryManager(const QString &storagePath = {});

    bool write(const MemoryEntry &entry);
    QList<MemoryEntry> search(const QString &query, int maxCount = 8) const;
    QList<MemoryEntry> entries() const;
    bool remove(const QString &idOrKey);
    QString userName() const;
    QString storagePath() const;

private:
    QString resolvedStoragePath() const;
    void ensureFresh() const;
    void loadFromDisk() const;
    bool saveToDisk() const;
    MemoryEntry normalize(const MemoryEntry &entry) const;
    bool shouldReject(const MemoryEntry &entry) const;

    QString m_storagePath;
    mutable QList<MemoryEntry> m_entries;
    mutable QDateTime m_lastFileModified;
};
