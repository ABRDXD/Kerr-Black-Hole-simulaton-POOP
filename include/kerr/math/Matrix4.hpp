#pragma once

#include <array>
#include <cstddef>

namespace kerr::math {

template <typename Scalar>
struct Matrix4 final {
    std::array<Scalar, 16> elements{};

    [[nodiscard]] constexpr Scalar& operator()(const std::size_t row,
                                                const std::size_t column) noexcept {
        return elements[row * 4U + column];
    }

    [[nodiscard]] constexpr const Scalar& operator()(const std::size_t row,
                                                      const std::size_t column) const noexcept {
        return elements[row * 4U + column];
    }

    [[nodiscard]] static constexpr Matrix4 identity() noexcept {
        Matrix4 result{};
        for (std::size_t index = 0; index < 4U; ++index) {
            result(index, index) = Scalar{1};
        }
        return result;
    }
};

using Matrix4d = Matrix4<double>;

} // namespace kerr::math

