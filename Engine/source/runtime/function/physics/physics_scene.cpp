#include "physics_scene.h"

namespace vesper {

PhysicsScene::PhysicsScene() = default;
PhysicsScene::~PhysicsScene() = default;

void PhysicsScene::initialize() {
    // TODO: Initialize Jolt Physics
}

void PhysicsScene::shutdown() {
    // TODO: Cleanup Jolt Physics
}

void PhysicsScene::tick(float delta_time) {
    // TODO: Step physics simulation (fixed timestep)
}

uint32_t PhysicsScene::createRigidBody(/* TODO: RigidBodyDesc */) {
    // TODO: Create Jolt body
    return 0;
}

void PhysicsScene::destroyRigidBody(uint32_t body_id) {
    // TODO: Remove body from physics scene
}

void PhysicsScene::updateBodyTransform(uint32_t body_id, const float* transform) {
    // TODO: Update body transform
}

bool PhysicsScene::raycast(const float* origin, const float* direction, float max_distance, float* hit_point) {
    // TODO: Perform raycast
    return false;
}

void PhysicsScene::setGravity(float x, float y, float z) {
    // TODO: Set gravity
}

} // namespace vesper
