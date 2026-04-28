#pragma once

#include <cassert>
#include <cstdint>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ===== Sized Ints =====

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

// ===== Vectors =====

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using uVec3 = glm::uvec3;

using Mat4 = glm::mat4;

constexpr u64 KiloByte = 1'024;
constexpr u64 MegaByte = 1'024 * KiloByte;
constexpr u64 GigaByte = 1'024 * MegaByte;