#include "physics_manager.h"
#include "physics_scene.h"

namespace vesper {

PhysicsManager::PhysicsManager() = default;
PhysicsManager::~PhysicsManager() = default;

void PhysicsManager::initialize() {
    m_physics_scene = std::make_unique<PhysicsScene>();
    m_physics_scene->initialize();
}

void PhysicsManager::shutdown() {
    if (m_physics_scene) {
        m_physics_scene->shutdown();
        m_physics_scene.reset();
    }
}

void PhysicsManager::tick(float delta_time) {
    if (m_physics_scene) {
        m_physics_scene->tick(delta_time);
    }
}

PhysicsScene* PhysicsManager::getPhysicsScene() const {
    return m_physics_scene.get();
}

} // namespace vesper
