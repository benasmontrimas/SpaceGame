#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numbers>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
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

constexpr u32 u32_max = UINT32_MAX;
constexpr u64 u64_max = UINT64_MAX;

// ===== Vectors + Matrices ===== //

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using dVec2 = glm::dvec2;
using dVec3 = glm::dvec3;
using dVec4 = glm::dvec4;

using uVec3 = glm::uvec3;
using uVec4 = glm::uvec4;

using Mat4 = glm::mat4;

// ===== Constants ===== //

constexpr float  PI_F = std::numbers::pi_v<float>;
constexpr double PI_D = std::numbers::pi_v<double>;

constexpr u64 KiloByte = 1'024;
constexpr u64 MegaByte = 1'024 * KiloByte;
constexpr u64 GigaByte = 1'024 * MegaByte;

// ===== Structs ===== //

struct AABB {
        Vec3 center;
        Vec3 radius;

        // Return the closest point from p to the AABB
        Vec3 ClosestPoint(Vec3 p) {
                Vec3 res{};

                Vec3 min = (center - radius);
                Vec3 max = (center + radius);

                for (u32 i = 0; i < 3; i++) {
                        res[i] = p[i];
                        if (res[i] < min[i]) res[i] = min[i];
                        if (res[i] > max[i]) res[i] = max[i];
                }

                return res;
        }

        // Return the distance to the AABB
        float Distance(Vec3 p) {
                float res;

                Vec3 min = (center - radius);
                Vec3 max = (center + radius);

                for (u32 i = 0; i < 3; i++) {
                        if (p[i] < min[i]) res += (min[i] - p[i]) * (min[i] - p[i]);
                        if (p[i] > max[i]) res += (p[i] - max[i]) * (p[i] - max[i]);
                }

                return glm::sqrt(res);
        }

        // Return the distance to the AABB
        float DistanceSq(Vec3 p) {
                float res;

                Vec3 min = (center - radius);
                Vec3 max = (center + radius);

                for (u32 i = 0; i < 3; i++) {
                        if (p[i] < min[i]) res += (min[i] - p[i]) * (min[i] - p[i]);
                        if (p[i] > max[i]) res += (p[i] - max[i]) * (p[i] - max[i]);
                }

                return res;
        }
};