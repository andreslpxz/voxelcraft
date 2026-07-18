#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>

namespace vc {

// ===================== Vec2 =====================
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

// ===================== Vec3 =====================
struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float v) : x(v), y(v), z(v) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(float s) const { return {x/s, y/s, z/s}; }
    Vec3 operator*(const Vec3& o) const { return {x*o.x, y*o.y, z*o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }

    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    float lengthSq() const { return x*x + y*y + z*z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float l = length();
        if (l < 1e-8f) return {0,0,0};
        return {x/l, y/l, z/l};
    }
};

inline Vec3 normalize(const Vec3& v) { return v.normalized(); }
inline float dot(const Vec3& a, const Vec3& b) { return a.dot(b); }
inline Vec3 cross(const Vec3& a, const Vec3& b) { return a.cross(b); }
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return {a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t};
}

// ===================== Vec4 =====================
struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}

    Vec4 operator+(const Vec4& o) const { return {x+o.x, y+o.y, z+o.z, w+o.w}; }
    Vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
};

// ===================== Mat4 (column-major, like Vulkan) =====================
struct Mat4 {
    float m[16]; // column-major: m[col*4 + row]

    Mat4() {
        for (int i = 0; i < 16; i++) m[i] = 0;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 identity() { return Mat4(); }

    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scaling(const Vec3& s) {
        Mat4 r;
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }

    static Mat4 rotationX(float rad) {
        Mat4 r;
        float c = std::cos(rad), s = std::sin(rad);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 rotationY(float rad) {
        Mat4 r;
        float c = std::cos(rad), s = std::sin(rad);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }
    static Mat4 rotationZ(float rad) {
        Mat4 r;
        float c = std::cos(rad), s = std::sin(rad);
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // Perspective projection (Vulkan NDC: depth [0,1], Y down)
    static Mat4 perspective(float fovyRad, float aspect, float nearZ, float farZ) {
        Mat4 r;
        float f = 1.0f / std::tan(fovyRad * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = -f; // Y flipped for Vulkan
        r.m[10] = farZ / (farZ - nearZ);
        r.m[11] = 1.0f;
        r.m[14] = -(farZ * nearZ) / (farZ - nearZ);
        r.m[15] = 0.0f;
        return r;
    }

    // Look-at (right-handed, like gluLookAt)
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = cross(f, up).normalized();
        Vec3 u = cross(s, f);
        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -s.dot(eye);
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -u.dot(eye);
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] =  f.dot(eye);
        r.m[3] = 0;    r.m[7] = 0;    r.m[11] = 0;    r.m[15] = 1;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; c++) {
            for (int row = 0; row < 4; row++) {
                float sum = 0;
                for (int k = 0; k < 4; k++) {
                    sum += m[k*4 + row] * o.m[c*4 + k];
                }
                r.m[c*4 + row] = sum;
            }
        }
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        return {
            m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12],
            m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13],
            m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]
        };
    }

    Vec3 transformDir(const Vec3& d) const {
        return {
            m[0]*d.x + m[4]*d.y + m[8]*d.z,
            m[1]*d.x + m[5]*d.y + m[9]*d.z,
            m[2]*d.x + m[6]*d.y + m[10]*d.z
        };
    }
};

// ===================== Utility math =====================
inline float clamp(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float toRadians(float deg) { return deg * 3.14159265358979323846f / 180.0f; }
inline float toDegrees(float rad) { return rad * 180.0f / 3.14159265358979323846f; }

inline float smoothstep(float e0, float e1, float x) {
    float t = clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

} // namespace vc
