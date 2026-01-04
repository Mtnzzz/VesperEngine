#pragma once

#include <memory>

namespace vesper {

class PhysicsScene;

class PhysicsManager {
public:
    PhysicsManager();
    ~PhysicsManager();

    void initialize();
    void shutdown();

    void tick(float delta_time);

    PhysicsScene* getPhysicsScene() const;

private:
    std::unique_ptr<PhysicsScene> m_physics_scene;
};

} // namespace vesper