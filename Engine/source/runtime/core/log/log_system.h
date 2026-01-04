#pragma once

#include <spdlog/spdlog.h>

#include <cstdint>
#include <memory>
#include <stdexcept>

namespace vesper {

/// @brief Log severity levels
enum class LogLevel : uint8_t
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

/// @brief Async logging system based on spdlog
/// Supports console and file output, fatal level throws exception
class LogSystem final
{
public:
    LogSystem();
    ~LogSystem();

    LogSystem(const LogSystem&) = delete;
    LogSystem& operator=(const LogSystem&) = delete;

    /// @brief Log a message with the specified level
    /// @tparam ...Args Format argument types
    /// @param level Log severity level
    /// @param fmt Format string (fmt library syntax)
    /// @param ...args Format arguments
    template<typename... Args>
    void log(LogLevel level, spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        switch (level)
        {
            case LogLevel::Trace:
                m_logger->trace(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::Debug:
                m_logger->debug(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::Info:
                m_logger->info(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::Warn:
                m_logger->warn(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::Error:
                m_logger->error(fmt, std::forward<Args>(args)...);
                break;
            case LogLevel::Fatal:
                m_logger->critical(fmt, std::forward<Args>(args)...);
                fatalCallback(fmt, std::forward<Args>(args)...);
                break;
        }
    }

    /// @brief Set the minimum log level to display
    void setLevel(LogLevel level);

    /// @brief Flush all pending log messages
    void flush();

private:
    template<typename... Args>
    void fatalCallback(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        const std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        throw std::runtime_error(message);
    }

    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace vesper

// =============================================================================
// Global logger access - must be defined by application
// =============================================================================
namespace vesper {
    LogSystem* getLogSystem();
}

// =============================================================================
// Logging Macros
// =============================================================================

#define LOG_TRACE(...) \
    if (::vesper::getLogSystem()) { ::vesper::getLogSystem()->log(::vesper::LogLevel::Trace, __VA_ARGS__); }

#define LOG_DEBUG(...) \
    if (::vesper::getLogSystem()) { ::vesper::getLogSystem()->log(::vesper::LogLevel::Debug, __VA_ARGS__); }

#define LOG_INFO(...) \
    if (::vesper::getLogSystem()) { ::vesper::getLogSystem()->log(::vesper::LogLevel::Info, __VA_ARGS__); }

#define LOG_WARN(...) \
    if (::vesper::getLogSystem()) { ::vesper::getLogSystem()->log(::vesper::LogLevel::Warn, __VA_ARGS__); }

#define LOG_ERROR(...) \
    if (::vesper::getLogSystem()) { ::vesper::getLogSystem()->log(::vesper::LogLevel::Error, __VA_ARGS__); }

#define LOG_FATAL(...) \
    if (::vesper::getLogSystem()) { ::vesper::getLogSystem()->log(::vesper::LogLevel::Fatal, __VA_ARGS__); }