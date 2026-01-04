#pragma once

#include <memory>

namespace vesper {

class VesperEngine;

class VesperEditor {
public:
    VesperEditor();
    ~VesperEditor();

    void initialize();
    void run();
    void shutdown();

private:
    void initializeUI();
    void renderUI();

private:
    std::unique_ptr<VesperEngine> m_engine;
    bool m_is_running = false;
};

} // namespace vesper
