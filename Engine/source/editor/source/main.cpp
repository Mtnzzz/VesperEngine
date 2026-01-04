#include "runtime/engine.h"
#include "runtime/core/log/log_system.h"

#include <iostream>
#include <memory>

// Global log system instance
static std::unique_ptr<vesper::LogSystem> g_logSystem;

namespace vesper {
    LogSystem* getLogSystem() { return g_logSystem.get(); }
}

int main(int argc, char** argv)
{
    // Initialize log system first
    g_logSystem = std::make_unique<vesper::LogSystem>();

    int exitCode = 0;

    try
    {
        // Create and configure engine
        vesper::VesperEngine engine;

        vesper::EngineConfig config;
        config.windowWidth = 1920;
        config.windowHeight = 1080;
        config.windowTitle = "Vesper Engine";
        config.resizable = true;
        config.fullscreen = false;

        // Initialize and run
        if (engine.initialize(config))
        {
            engine.run();
            engine.shutdown();
        }
        else
        {
            exitCode = 1;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Fatal error: {}", e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        exitCode = 1;
    }

    // Shutdown log system
    g_logSystem.reset();

    return exitCode;
}