#include "editor.h"
#include "engine.h"

namespace vesper {

VesperEditor::VesperEditor() = default;
VesperEditor::~VesperEditor() = default;

void VesperEditor::initialize() {
    m_engine = std::make_unique<VesperEngine>();
    m_engine->initialize();
    initializeUI();
}

void VesperEditor::run() {
    m_is_running = true;
    while (m_is_running) {
        // TODO: Main editor loop
        renderUI();
    }
}

void VesperEditor::shutdown() {
    if (m_engine) {
        m_engine->shutdown();
        m_engine.reset();
    }
}

void VesperEditor::initializeUI() {
    // TODO: Initialize ImGui
}

void VesperEditor::renderUI() {
    // TODO: Render editor UI with ImGui
}

} // namespace vesper
