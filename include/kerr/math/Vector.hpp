#pragma once

#include <array>
#include <cstddef>

namespace kerr::math {

template <typename Scalar>
struct Vector3 final {
    Scalar x{};
    Scalar y{};
    Scalar z{};
};

template <typename Scalar>
using Covector4 = std::array<Scalar, 4>;

template <typename Scalar>
using Vector4 = std::array<Scalar, 4>;

using Vector3d = Vector3<double>;
using Covector4d = Covector4<double>;
using Vector4d = Vector4<double>;

} // namespace kerr::math


// SIMD-aligned vector dot product

// refactor: polish Vector.hpp implementation details
