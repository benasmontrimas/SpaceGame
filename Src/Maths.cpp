#include "Maths.h"
#include <glm/gtx/intersect.hpp>

bool RayAABB(Ray ray, AABB aabb) {
        Vec3 aabb_min = aabb.center - aabb.radius;
        Vec3 aabb_max = aabb.center + aabb.radius;

        Vec3 inverse_dir = 1.0f / ray.direction;

        float tx1 = (aabb_min.x - ray.origin.x)*inverse_dir.x;
        float tx2 = (aabb_max.x - ray.origin.x)*inverse_dir.x;

        float tmin = std::min(tx1, tx2);
        float tmax = std::max(tx1, tx2);

        float ty1 = (aabb_min.y - ray.origin.y)*inverse_dir.y;
        float ty2 = (aabb_max.y - ray.origin.y)*inverse_dir.y;

        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));

        float tz1 = (aabb_min.z - ray.origin.z)*inverse_dir.z;
        float tz2 = (aabb_max.z - ray.origin.z)*inverse_dir.z;

        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));

        return tmax >= tmin;

        // Vec3 extents;
        // Vec3 diff;

        // for (u32 axis = 0; axis < 3; axis++) {
        //         diff[axis]    = ray.origin[axis] - aabb.center[axis];
        //         extents[axis] = aabb.radius[axis];
        //         if (fabsf(diff[axis]) > extents[axis] and diff[axis] * ray.direction[axis] >= 0.0f) return false;
        // }

        // Vec3 abs_ray_direction = glm::abs(ray.direction);

        // float f;

        // f = ray.direction.y * diff.z - ray.direction.z * diff.y;
        // if (fabsf(f) > extents.y * abs_ray_direction.z + extents.z * abs_ray_direction.y) return false;

        // f = ray.direction.z * diff.x - ray.direction.x * diff.z;
        // if (fabsf(f) > extents.x * abs_ray_direction.z + extents.z * abs_ray_direction.x) return false;

        // f = ray.direction.x * diff.y - ray.direction.y * diff.x;
        // if (fabsf(f) > extents.x * abs_ray_direction.y + extents.y * abs_ray_direction.x) return false;

        // return true;
}

float RayTriangle(Ray ray, Vec3 v0, Vec3 v1, Vec3 v2) {
        Vec3 position;
        if (!glm::intersectLineTriangle(ray.origin, ray.direction, v0, v1, v2, position)) return f32_max;
        return glm::length(v0 - ray.origin);
}