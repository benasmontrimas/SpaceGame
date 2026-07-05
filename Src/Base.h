#pragma once

#include <cassert>
#include <cstdint>
#include <format>
#include <numbers>
#include <print>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

// ===== Sized Ints ===== //

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using uintptr = uintptr_t;

constexpr u32   u32_max = UINT32_MAX;
constexpr u64   u64_max = UINT64_MAX;
constexpr float f32_max = FLT_MAX;

// ===== Constants ===== //

constexpr float  PI_F = std::numbers::pi_v<float>;
constexpr double PI_D = std::numbers::pi_v<double>;

constexpr u64 KiloByte = 1'024;
constexpr u64 MegaByte = 1'024 * KiloByte;
constexpr u64 GigaByte = 1'024 * MegaByte;


// ===== Vectors + Matrices ===== //

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using dVec2 = glm::dvec2;
using dVec3 = glm::dvec3;
using dVec4 = glm::dvec4;

using uVec3 = glm::uvec3;
using uVec4 = glm::uvec4;

using iVec3 = glm::ivec3;
using iVec4 = glm::ivec4;

using Mat4 = glm::mat4;

using Quat = glm::quat;

template <>
struct std::formatter<Vec2> : std::formatter<std::string_view> {
        auto format(const Vec2& v, std::format_context& ctx) const {
                return std::format_to(ctx.out(), "({}, {})", v.x, v.y);
        }
};

template <>
struct std::formatter<Vec3> : std::formatter<std::string_view> {
        auto format(const Vec3& v, std::format_context& ctx) const {
                return std::format_to(ctx.out(), "({}, {}, {})", v.x, v.y, v.z);
        }
};

template <>
struct std::formatter<Vec4> : std::formatter<std::string_view> {
        auto format(const Vec4& v, std::format_context& ctx) const {
                return std::format_to(ctx.out(), "({}, {}, {}, {})", v.x, v.y, v.z, v.w);
        }
};

// Represents a object transform in 3D.
struct Transform {
        Vec3 position{};
        Quat rotation{};
        Vec3 scale{ 1, 1, 1 };
};

// Represents a object transform in 2D.
struct Transform2D {
        Vec2  position{};
        float rotation{};
        Vec2  scale{ 1, 1 };
};

struct AABB {
        Vec3 center;
        Vec3 radius{ -f32_max, -f32_max, -f32_max };

        float Area() const {
                Vec3 size = radius * 2.0f;
                return ((size.x * size.y) + (size.y * size.z) + (size.x * size.z)) * 2.0f;
        }

        void Extend(Vec3 p) {
                Vec3 min_bounds = center - radius;
                Vec3 max_bounds = center + radius;

                max_bounds = glm::max(max_bounds, p);
                min_bounds = glm::min(min_bounds, p);

                radius = (max_bounds - min_bounds) / 2.0f;
                center = min_bounds + radius;
        }

        void Extend(AABB other) {
                Vec3 min_bounds = center - radius;
                Vec3 max_bounds = center + radius;

                Vec3 other_min_bounds = other.center - other.radius;
                Vec3 other_max_bounds = other.center + other.radius;

                max_bounds = glm::max(max_bounds, other_max_bounds);
                min_bounds = glm::min(min_bounds, other_min_bounds);

                radius = (max_bounds - min_bounds) / 2.0f;
                center = min_bounds + radius;
        }

        // Return the closest point from p to the AABB
        Vec3 ClosestPoint(Vec3 p) {
                Vec3 res{};

                Vec3 min_bounds = (center - radius);
                Vec3 max_bounds = (center + radius);

                for (u32 i = 0; i < 3; i++) {
                        res[i] = p[i];
                        if (res[i] < min_bounds[i]) res[i] = min_bounds[i];
                        if (res[i] > max_bounds[i]) res[i] = max_bounds[i];
                }

                return res;
        }

        // Return the distance to the AABB
        float Distance(Vec3 p) {
                float res{};

                Vec3 min_bounds = (center - radius);
                Vec3 max_bounds = (center + radius);

                for (u32 i = 0; i < 3; i++) {
                        if (p[i] < min_bounds[i]) res += (min_bounds[i] - p[i]) * (min_bounds[i] - p[i]);
                        if (p[i] > max_bounds[i]) res += (p[i] - max_bounds[i]) * (p[i] - max_bounds[i]);
                }

                return glm::sqrt(res);
        }

        // Return the distance to the AABB
        float DistanceSq(Vec3 p) {
                float res{};

                Vec3 min_bounds = (center - radius);
                Vec3 max_bounds = (center + radius);

                for (u32 i = 0; i < 3; i++) {
                        if (p[i] < min_bounds[i]) res += (min_bounds[i] - p[i]) * (min_bounds[i] - p[i]);
                        if (p[i] > max_bounds[i]) res += (p[i] - max_bounds[i]) * (p[i] - max_bounds[i]);
                }

                return res;
        }
};

struct Ray {
        Vec3 origin;
        Vec3 direction;
};

bool  RayAABB(Ray ray, AABB aabb);
float RayTriangle(Ray ray, Vec3 v0, Vec3 v1, Vec3 v2);