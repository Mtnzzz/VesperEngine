#include "log_system.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vesper {

LogSystem::LogSystem()
{
    // Console sink with colored output
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    // File sink for persistent logging
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("vesper.log", true);
    fileSink->set_level(spdlog::level::trace);
    fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");

    // Initialize async thread pool: 8192 queue size, 1 background thread
    spdlog::init_thread_pool(8192, 1);

    // Create async logger with both sinks
    std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
    m_logger = std::make_shared<spdlog::async_logger>(
        "vesper",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );

    m_logger->set_level(spdlog::level::trace);
    spdlog::register_logger(m_logger);
    spdlog::set_default_logger(m_logger);

    m_logger->info("VesperEngine LogSystem initialized");
}

LogSystem::~LogSystem()
{
    m_logger->info("VesperEngine LogSystem shutting down");
    flush();
    spdlog::drop_all();
    spdlog::shutdown();
}

void LogSystem::setLevel(LogLevel level)
{
    static constexpr spdlog::level::level_enum levelMap[] = {
        spdlog::level::trace,
        spdlog::level::debug,
        spdlog::level::info,
        spdlog::level::warn,
        spdlog::level::err,
        spdlog::level::critical
    };

    m_logger->set_level(levelMap[static_cast<uint8_t>(level)]);
}

void LogSystem::flush()
{
    m_logger->flush();
}

} // namespace vesper