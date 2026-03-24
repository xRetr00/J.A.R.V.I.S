#include "ai/StreamAssembler.h"

StreamAssembler::StreamAssembler(QObject *parent)
    : QObject(parent)
{
}

void StreamAssembler::reset()
{
    m_fullText.clear();
    m_pendingSentenceBuffer.clear();
}

void StreamAssembler::appendChunk(const QString &chunk)
{
    if (chunk.isEmpty()) {
        return;
    }

    m_fullText += chunk;
    m_pendingSentenceBuffer += chunk;
    emit partialTextUpdated(m_fullText);

    const int boundary = lastSentenceBoundary(m_pendingSentenceBuffer);
    if (boundary <= 0) {
        return;
    }

    const QString sentence = m_pendingSentenceBuffer.left(boundary).trimmed();
    m_pendingSentenceBuffer = m_pendingSentenceBuffer.mid(boundary).trimmed();
    if (!sentence.isEmpty()) {
        emit sentenceReady(sentence);
    }
}

QString StreamAssembler::fullText() const
{
    return m_fullText;
}

QString StreamAssembler::drainRemainingText()
{
    const QString remainder = m_pendingSentenceBuffer.trimmed();
    m_pendingSentenceBuffer.clear();
    return remainder;
}

int StreamAssembler::lastSentenceBoundary(const QString &text) const
{
    for (int i = text.size() - 1; i >= 0; --i) {
        const auto c = text.at(i);
        if (c == QChar::fromLatin1('.') || c == QChar::fromLatin1('!') || c == QChar::fromLatin1('?')) {
            if (i + 1 < text.size() && !text.at(i + 1).isSpace()) {
                return -1;
            }
            return i + 1;
        }
    }

    return -1;
}
