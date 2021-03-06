/*
 * Copyright (C) 2017-2018 Trent Houliston <trent@houliston.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VISUALMESH_UTILITY_MATH_HPP
#define VISUALMESH_UTILITY_MATH_HPP

#include <algorithm>
#include <array>
#include <cmath>

namespace visualmesh {

// Typedef some value types we commonly use
template <typename Scalar>
using vec2 = std::array<Scalar, 2>;
template <typename Scalar>
using vec3 = std::array<Scalar, 3>;
template <typename Scalar>
using vec4 = std::array<Scalar, 4>;
template <typename Scalar>
using mat3 = std::array<vec3<Scalar>, 3>;
template <typename Scalar>
using mat4 = std::array<vec4<Scalar>, 4>;

// I could use Eigen for this, but if I use just the stl, at least nobody will have library problems
template <typename Scalar>
inline constexpr Scalar dot(const vec3<Scalar>& a, const vec3<Scalar>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
template <typename Scalar>
inline constexpr Scalar dot(const vec4<Scalar>& a, const vec4<Scalar>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

template <typename Scalar>
inline constexpr vec3<Scalar> cross(const vec3<Scalar>& a, const vec3<Scalar>& b) {
  return {{
    a[1] * b[2] - a[2] * b[1],  // x
    a[2] * b[0] - a[0] * b[2],  // y
    a[0] * b[1] - a[1] * b[0]   // z
  }};
}

template <typename Scalar>
inline constexpr mat4<Scalar> transpose(const mat4<Scalar> mat) {
  return mat4<Scalar>{vec4<Scalar>{mat[0][0], mat[1][0], mat[2][0], mat[3][0]},
                      vec4<Scalar>{mat[0][1], mat[1][1], mat[2][1], mat[3][1]},
                      vec4<Scalar>{mat[0][2], mat[1][2], mat[2][2], mat[3][2]},
                      vec4<Scalar>{mat[0][3], mat[1][3], mat[2][3], mat[3][3]}};
}

template <typename Scalar>
inline constexpr vec3<Scalar> normalise(const vec3<Scalar>& a) {
  Scalar length = Scalar(1.0) / std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] + a[2]);
  return {{a[0] * length, a[1] * length, a[2] * length}};
}
}  // namespace visualmesh

#endif  // VISUALMESH_UTILITY_MATH_HPP
