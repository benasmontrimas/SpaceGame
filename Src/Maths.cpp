#include "Maths.h"
#include <glm/gtx/intersect.hpp>

bool RayAABB(Ray ray, AABB aabb) {
        Vec3 extents;
        Vec3 diff;

        for (u32 axis = 0; axis < 3; axis++) {
                diff[axis]    = ray.origin[axis] - aabb.center[axis];
                extents[axis] = aabb.radius[axis];
                if (fabsf(diff[axis]) > extents[axis] and diff[axis] * ray.direction[axis] >= 0.0f) return false;
        }

        Vec3 abs_ray_direction = glm::abs(ray.direction);

        float f;

        f = ray.direction.y * diff.z - ray.direction.z * diff.y;
        if (fabsf(f) > extents.y * abs_ray_direction.z + extents.z * abs_ray_direction.y) return false;

        f = ray.direction.z * diff.x - ray.direction.x * diff.z;
        if (fabsf(f) > extents.x * abs_ray_direction.z + extents.z * abs_ray_direction.x) return false;

        f = ray.direction.x * diff.y - ray.direction.y * diff.x;
        if (fabsf(f) > extents.x * abs_ray_direction.y + extents.y * abs_ray_direction.x) return false;

        return true;
}

float RayTriangle(Ray ray, Vec3 v0, Vec3 v1, Vec3 v2) {
        Vec2  bary_position;
        float distance;
        if (!glm::intersectRayTriangle(ray.origin, ray.direction, v0, v1, v2, bary_position, distance)) return f32_max;

        if (bary_position.x > 1.0f or bary_position.y > 1.0f or (1.0f - (bary_position.x + bary_position.y)) > 1.0f) return f32_max;
        if (distance < 0.0f) return f32_max;

        return distance;
}