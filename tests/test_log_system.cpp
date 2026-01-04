#include <gtest/gtest.h>

#include "runtime/core/log/log_system.h"

namespace vesper {
namespace test {

class LogSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Log system is initialized in test_main.cpp
    }
};

TEST_F(LogSystemTest, LogSystemExists) {
    EXPECT_NE(getLogSystem(), nullptr);
}

TEST_F(LogSystemTest, CanLogTrace) {
    EXPECT_NO_THROW({
        LOG_TRACE("Test trace message");
    });
}

TEST_F(LogSystemTest, CanLogDebug) {
    EXPECT_NO_THROW({
        LOG_DEBUG("Test debug message with value: {}", 42);
    });
}

TEST_F(LogSystemTest, CanLogInfo) {
    EXPECT_NO_THROW({
        LOG_INFO("Test info message");
    });
}

TEST_F(LogSystemTest, CanLogWarn) {
    EXPECT_NO_THROW({
        LOG_WARN("Test warning message");
    });
}

TEST_F(LogSystemTest, CanLogError) {
    EXPECT_NO_THROW({
        LOG_ERROR("Test error message with code: {}", -1);
    });
}

TEST_F(LogSystemTest, CanFormatMultipleTypes) {
    EXPECT_NO_THROW({
        LOG_INFO("int={}, float={:.2f}, string={}", 100, 3.14159f, "hello");
    });
}

TEST_F(LogSystemTest, CanChangeLogLevel) {
    EXPECT_NO_THROW({
        getLogSystem()->setLevel(LogLevel::Error);
        getLogSystem()->setLevel(LogLevel::Trace);
    });
}

TEST_F(LogSystemTest, FatalThrowsException) {
    EXPECT_THROW({
        LOG_FATAL("This should throw an exception");
    }, std::runtime_error);
}

} // namespace test
} // namespace vesper