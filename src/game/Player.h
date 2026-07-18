#pragma once
#include "Camera.h"
#include "InputManager.h"
#include "../world/World.h"
#include "../math/Math.h"

namespace vc {

struct Player {
    Vec3 position;          // eye position (camera)
    Vec3 velocity;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float width  = 0.6f;
    float height = 1.8f;    // total collision height
    float eyeHeight = 1.62f;
    bool  onGround = false;
    bool  flying   = false;
    bool  inWater  = false;

    // Movement parameters
    float walkSpeed    = 4.317f; // blocks/sec
    float flySpeed     = 12.0f;
    float jumpSpeed    = 8.0f;
    float gravity      = 24.0f;
    float sprintMultiplier = 1.3f;

    Camera camera;

    void updateCamera() {
        camera.position = position;
        camera.yaw = yaw;
        camera.pitch = pitch;
    }
};

// Raycast result for block selection
struct RaycastHit {
    bool     hit = false;
    Vec3     blockPos;     // integer block coords (use as int)
    Vec3     normal;       // face normal of hit block
    float    distance;
};

// DDA-based voxel raycast
RaycastHit raycastVoxels(World& world, const Vec3& origin, const Vec3& dir, float maxDist);

// Update player physics & collision against world
void updatePlayer(Player& p, World& world, const InputState& input, float dt);

// Apply look from input deltas (yaw/pitch)
void applyLook(Player& p, float dx, float dy, float sensitivity = 0.0025f);

} // namespace vc
