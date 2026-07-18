#pragma once
#include "../math/Math.h"
#include "../world/World.h"

namespace vc {

struct Camera {
    Vec3 position;
    float yaw   = 0.0f; // around Y, radians
    float pitch = 0.0f; // around X, radians (-PI/2 .. +PI/2)
    float fov   = 70.0f;
    float nearZ = 0.05f;
    float farZ  = 1000.0f;

    Vec3 forward() const {
        return Vec3(
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::cos(yaw)
        ).normalized();
    }
    Vec3 right() const {
        return Vec3(std::cos(yaw), 0, -std::sin(yaw));
    }
    Vec3 up() const { return Vec3(0, 1, 0); }

    Mat4 view() const {
        return Mat4::lookAt(position, position + forward(), up());
    }
    Mat4 projection(float aspect) const {
        return Mat4::perspective(toRadians(fov), aspect, nearZ, farZ);
    }
};

} // namespace vc
