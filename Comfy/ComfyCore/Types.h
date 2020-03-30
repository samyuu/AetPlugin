#pragma once
#include "IntegralTypes.h"
#include "FileAddr.h"
#include "Core/NonCopyable.h"
#include "Core/SmartPointers.h"
#include <glm/gtc/type_ptr.hpp>

// NOTE: Measure a frame unit / frame rate
using frame_t = float;

using bvec2 = glm::vec<2, bool, glm::defaultp>;
using bvec3 = glm::vec<3, bool, glm::defaultp>;
using bvec4 = glm::vec<4, bool, glm::defaultp>;

using ivec2 = glm::vec<2, int, glm::defaultp>;
using ivec3 = glm::vec<3, int, glm::defaultp>;
using ivec4 = glm::vec<4, int, glm::defaultp>;

using uvec2 = glm::vec<2, unsigned int, glm::defaultp>;
using uvec3 = glm::vec<3, unsigned int, glm::defaultp>;
using uvec4 = glm::vec<4, unsigned int, glm::defaultp>;

using vec2 = glm::vec<2, float, glm::defaultp>;
using vec3 = glm::vec<3, float, glm::defaultp>;
using vec4 = glm::vec<4, float, glm::defaultp>;

using dvec2 = glm::vec<2, double, glm::defaultp>;
using dvec3 = glm::vec<3, double, glm::defaultp>;
using dvec4 = glm::vec<4, double, glm::defaultp>;

using mat3 = glm::mat<3, 3, float, glm::defaultp>;
using mat4 = glm::mat<4, 4, float, glm::defaultp>;

using quat = glm::qua<float, glm::defaultp>;
