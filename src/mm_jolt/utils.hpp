#pragma once

#include <glm/vec3.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Math.h>


static inline glm::vec3 to_glm(const JPH::Vec3& vec) {
	return {vec.GetX(), vec.GetY(), vec.GetZ()};
}

static inline JPH::Vec3 to_jph(const glm::vec3& vec) {
	return {vec.x, vec.y, vec.z};
}

