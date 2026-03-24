#include "logging/LoggingService.h"

#include <QDir>
#include <QStandardPaths>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

LoggingService::LoggingService(QObject *parent)
    : QObject(parent)
{
}

bool LoggingService::initialize()
{
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/logs");
    QDir().mkpath(root);

    try {
        m_logger = spdlog::rotating_logger_mt(
            "jarvis",
            (root + QStringLiteral("/jarvis.log")).toStdString(),
            1024 * 1024 * 5,
            3);
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
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
