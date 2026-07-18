#include "Player.h"
#include "InputManager.h"
#include <cmath>
#include <algorithm>

namespace vc {

static bool blockCollides(World& world, const Vec3& pos, float w, float h) {
    // AABB: pos is feet center; extends w/2 in X/Z, full h in Y
    float minX = pos.x - w/2, maxX = pos.x + w/2;
    float minY = pos.y,      maxY = pos.y + h;
    float minZ = pos.z - w/2, maxZ = pos.z + w/2;
    int x0 = (int)std::floor(minX), x1 = (int)std::floor(maxX);
    int y0 = (int)std::floor(minY), y1 = (int)std::floor(maxY);
    int z0 = (int)std::floor(minZ), z1 = (int)std::floor(maxZ);
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++)
            for (int z = z0; z <= z1; z++) {
                BlockId b = world.getBlock(x, y, z);
                if (isSolid(b)) return true;
            }
    return false;
}

void applyLook(Player& p, float dx, float dy, float sensitivity) {
    p.yaw   -= dx * sensitivity;
    p.pitch -= dy * sensitivity;
    const float maxPitch = 1.55334f; // ~89 deg
    if (p.pitch >  maxPitch) p.pitch =  maxPitch;
    if (p.pitch < -maxPitch) p.pitch = -maxPitch;
}

RaycastHit raycastVoxels(World& world, const Vec3& origin, const Vec3& dir, float maxDist) {
    RaycastHit r;
    Vec3 d = dir.normalized();
    int ix = (int)std::floor(origin.x);
    int iy = (int)std::floor(origin.y);
    int iz = (int)std::floor(origin.z);

    float stepX = (d.x > 0) ? 1.0f : -1.0f;
    float stepY = (d.y > 0) ? 1.0f : -1.0f;
    float stepZ = (d.z > 0) ? 1.0f : -1.0f;

    auto tDelta = [](float d) {
        return (d == 0.0f) ? 1e30f : std::abs(1.0f / d);
    };
    float dxT = tDelta(d.x);
    float dyT = tDelta(d.y);
    float dzT = tDelta(d.z);

    auto tMax = [&](float o, int i, float step, float d) -> float {
        if (d == 0) return 1e30f;
        float boundary = (step > 0) ? (i + 1.0f) : (float)i;
        return (boundary - o) / d;
    };
    float mx = tMax(origin.x, ix, stepX, d.x);
    float my = tMax(origin.y, iy, stepY, d.y);
    float mz = tMax(origin.z, iz, stepZ, d.z);

    float t = 0.0f;
    int face = -1; // 0..5

    while (t <= maxDist) {
        BlockId b = world.getBlock(ix, iy, iz);
        if (isSolid(b) || b == BLOCK_WATER) {
            r.hit = true;
            r.blockPos = Vec3((float)ix, (float)iy, (float)iz);
            r.distance = t;
            switch (face) {
                case 0: r.normal = Vec3(-stepX, 0, 0); break;
                case 1: r.normal = Vec3(0, -stepY, 0); break;
                case 2: r.normal = Vec3(0, 0, -stepZ); break;
                default: r.normal = Vec3(0, 0, 0);
            }
            return r;
        }
        if (mx < my && mx < mz) {
            ix += (int)stepX; t = mx; mx += dxT; face = 0;
        } else if (my < mz) {
            iy += (int)stepY; t = my; my += dyT; face = 1;
        } else {
            iz += (int)stepZ; t = mz; mz += dzT; face = 2;
        }
    }
    return r;
}

void updatePlayer(Player& p, World& world, const InputState& input, float dt) {
    // Look
    applyLook(p, input.lookDX, input.lookDY);

    // Determine movement direction in world space
    Vec3 forward(p.camera.forward().x, 0, p.camera.forward().z);
    forward = forward.normalized();
    Vec3 right(p.camera.right().x, 0, p.camera.right().z);
    right = right.normalized();

    Vec3 move(0, 0, 0);
    float jx = input.moveDX;
    float jy = input.moveDY;
    // joystick y up = forward
    move += forward * (-jy);
    move += right   * (jx);

    if (move.lengthSq() > 1.0f) move = move.normalized();

    // Check if in water
    BlockId headBlock = world.getBlock((int)p.position.x, (int)(p.position.y + 0.1f), (int)p.position.z);
    BlockId feetBlock = world.getBlock((int)p.position.x, (int)(p.position.y - 0.1f), (int)p.position.z);
    p.inWater = (headBlock == BLOCK_WATER || feetBlock == BLOCK_WATER);

    float speed = p.flying ? p.flySpeed : (p.inWater ? p.walkSpeed * 0.5f : p.walkSpeed);

    if (p.flying) {
        Vec3 desired = move * speed;
        // Smoothly approach desired velocity
        p.velocity.x = desired.x;
        p.velocity.z = desired.z;
        // Vertical: jump = up, crouch (no input yet) = down. For now use joystick Y > 0.8 to ascend.
        p.velocity.y = 0;
        if (input.jumpHeld) p.velocity.y = speed;
        else if (jy > 0.6f) p.velocity.y = -speed;
    } else {
        Vec3 desired = move * speed;
        // Apply horizontal velocity directly (arcade feel)
        p.velocity.x = desired.x;
        p.velocity.z = desired.z;
        // Gravity
        if (p.inWater) {
            p.velocity.y -= 6.0f * dt;             // gentle sink
            if (p.velocity.y < -3.0f) p.velocity.y = -3.0f;
            if (input.jumpHeld) p.velocity.y = 3.0f; // swim up
        } else {
            p.velocity.y -= p.gravity * dt;
            if (input.jumpPressed && p.onGround) {
                p.velocity.y = p.jumpSpeed;
                p.onGround = false;
            }
        }
    }

    // Integrate with collision (per-axis sweep)
    Vec3 newPos = p.position;
    // X axis
    Vec3 tryX = newPos + Vec3(p.velocity.x * dt, 0, 0);
    if (!blockCollides(world, tryX, p.width, p.height)) newPos.x = tryX.x;
    else p.velocity.x = 0;
    // Z axis
    Vec3 tryZ = newPos + Vec3(0, 0, p.velocity.z * dt);
    if (!blockCollides(world, tryZ, p.width, p.height)) newPos.z = tryZ.z;
    else p.velocity.z = 0;
    // Y axis
    Vec3 tryY = newPos + Vec3(0, p.velocity.y * dt, 0);
    if (!blockCollides(world, tryY, p.width, p.height)) {
        newPos.y = tryY.y;
        p.onGround = false;
    } else {
        if (p.velocity.y < 0) p.onGround = true;
        p.velocity.y = 0;
    }

    // Clamp to world bounds
    if (newPos.y < 1) {
        newPos.y = 1;
        p.velocity.y = 0;
        p.onGround = true;
    }

    p.position = newPos;
    p.updateCamera();
}

} // namespace vc
