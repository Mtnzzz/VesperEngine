#pragma once

#include <cstdint>

namespace vesper {

class PhysicsScene {
public:
    PhysicsScene();
    ~PhysicsScene();

    void initialize();
    void shutdown();

    void tick(float delta_time);

    // Body management
    uint32_t createRigidBody(/* TODO: RigidBodyDesc */);
    void destroyRigidBody(uint32_t body_id);
    void updateBodyTransform(uint32_t body_id, const float* transform);

    // Queries
    bool raycast(const float* origin, const float* direction, float max_distance, /* out */ float* hit_point);

    // Settings
    void setGravity(float x, float y, float z);

private:
    // TODO: Jolt Physics handles
    // JPH::PhysicsSystem* m_physics_system;
    // JPH::TempAllocator* m_temp_allocator;
    // JPH::JobSystem* m_job_system;
};

} // namespace vesper
