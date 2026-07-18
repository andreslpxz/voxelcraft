#pragma once
#include "../math/Math.h"

namespace vc {

// 20-minute day cycle (like Minecraft default)
class DayNightCycle {
public:
    float timeOfDay = 0.25f; // 0..1, 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
    float dayLength = 1200.0f; // seconds for full cycle

    void update(float dt) {
        timeOfDay += dt / dayLength;
        if (timeOfDay >= 1.0f) timeOfDay -= 1.0f;
    }

    // Sun direction (normalized) — moves in an arc across the sky
    Vec3 sunDirection() const {
        // Sun angle: 0 at sunrise, PI at sunset, 2PI back to sunrise
        float angle = timeOfDay * 2.0f * 3.14159265f;
        // Sun rotates around X axis: at noon it's straight up
        return Vec3(
            0,
            -std::cos(angle),
            -std::sin(angle)
        ).normalized();
    }

    // Returns 0 (night) to 1 (full day) brightness multiplier
    float daylight() const {
        Vec3 s = sunDirection();
        // brightness based on sun height (y component)
        float h = -s.y; // >0 during day
        return clamp(smoothstep(-0.15f, 0.25f, h), 0.05f, 1.0f);
    }

    // Ambient color tint (cool at night, warm at day, orange at sunset)
    Vec3 skyColor() const {
        Vec3 dayColor   (0.53f, 0.81f, 0.98f);
        Vec3 nightColor (0.03f, 0.04f, 0.10f);
        Vec3 sunsetColor(0.95f, 0.42f, 0.21f);

        float t = daylight();
        Vec3 base = lerp(nightColor, dayColor, t);
        // Sunset/sunrise tint
        Vec3 s = sunDirection();
        float horizonness = 1.0f - std::abs(s.y);
        float sunsetAmt = horizonness * (1.0f - std::abs(timeOfDay - 0.25f) * 4.0f > 0 ? 1.0f : 0.0f)
                         + horizonness * (1.0f - std::abs(timeOfDay - 0.75f) * 4.0f > 0 ? 1.0f : 0.0f);
        sunsetAmt = clamp(sunsetAmt, 0.0f, 1.0f) * (1.0f - t);
        return lerp(base, sunsetColor, sunsetAmt * 0.6f);
    }

    Vec3 fogColor() const { return skyColor(); }
};

} // namespace vc
