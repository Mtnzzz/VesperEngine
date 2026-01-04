#include <gtest/gtest.h>

#include "runtime/core/log/log_system.h"

#include <memory>

// Global log system for tests
static std::unique_ptr<vesper::LogSystem> g_testLogSystem;

namespace vesper {
    LogSystem* getLogSystem() { return g_testLogSystem.get(); }
}

class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize log system before all tests
        g_testLogSystem = std::make_unique<vesper::LogSystem>();
        g_testLogSystem->setLevel(vesper::LogLevel::Warn); // Reduce noise during tests
    }

    void TearDown() override {
        // Cleanup log system after all tests
        g_testLogSystem.reset();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Register test environment
    ::testing::AddGlobalTestEnvironment(new TestEnvironment());

    return RUN_ALL_TESTS();
}