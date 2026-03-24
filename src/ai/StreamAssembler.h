#pragma once

#include <QObject>
#include <QString>

class StreamAssembler : public QObject
{
    Q_OBJECT

public:
    explicit StreamAssembler(QObject *parent = nullptr);

    void reset();
    void appendChunk(const QString &chunk);
    QString fullText() const;
    QString drainRemainingText();

signals:
    void partialTextUpdated(const QString &text);
    void sentenceReady(const QString &sentence);

private:
    int lastSentenceBoundary(const QString &text) const;

    QString m_fullText;
    QString m_pendingSentenceBuffer;
};
