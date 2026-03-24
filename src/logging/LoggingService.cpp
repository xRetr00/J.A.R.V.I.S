#include "logging/LoggingService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QStringConverter>
#include <QTextStream>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

LoggingService::LoggingService(QObject *parent)
    : QObject(parent)
{
}

bool LoggingService::initialize()
{
    const auto root = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");
    QDir().mkpath(root);
    m_logFilePath = root + QStringLiteral("/jarvis.log");

    try {
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            m_logFilePath.toStdString(),
            1024 * 1024 * 5,
            3);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        m_logger = std::make_shared<spdlog::logger>("jarvis", sinks.begin(), sinks.end());
        spdlog::register_logger(m_logger);
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        m_logger->set_level(spdlog::level::info);
        m_logger->flush_on(spdlog::level::info);
        return true;
    } catch (...) {
        return false;
    }
}

void LoggingService::info(const QString &message) const
{
    if (m_logger) {
        m_logger->info(message.toStdString());
    }
}

void LoggingService::warn(const QString &message) const
{
    if (m_logger) {
        m_logger->warn(message.toStdString());
    }
}

void LoggingService::error(const QString &message) const
{
    if (m_logger) {
        m_logger->error(message.toStdString());
    }
}

bool LoggingService::logAiExchange(const QString &prompt, const QString &response, const QString &source, const QString &status) const
{
    const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/logs/AI");
    if (!QDir().mkpath(root)) {
        return false;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString randomToken = QString::number(QRandomGenerator::global()->generate(), 16);
    const QString path = root + QStringLiteral("/") + timestamp + QStringLiteral("_") + randomToken + QStringLiteral(".log");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "timestamp=" << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n";
    out << "source=" << source << "\n";
    if (!status.trimmed().isEmpty()) {
        out << "status=" << status.trimmed() << "\n";
    }
    out << "\n[PROMPT]\n";
    out << prompt.trimmed() << "\n";
    out << "\n[RESPONSE]\n";
    out << response.trimmed() << "\n";
    out.flush();
    file.close();

    if (m_logger) {
        m_logger->info(QStringLiteral("AI exchange log written: %1").arg(path).toStdString());
    }
    return true;
}

QString LoggingService::logFilePath() const
{
    return m_logFilePath;
}
