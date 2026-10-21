#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace kerr::math {

// Forward-mode differentiation: one value and N independent partials.
template <std::size_t N> struct Dual final {
    long double value{};
    std::array<long double, N> gradient{};

    constexpr Dual() = default;
    constexpr Dual(long double scalar) : value{scalar} {} // NOLINT implicit constant

    [[nodiscard]] static constexpr Dual variable(long double scalar, std::size_t axis) {
        Dual result{scalar};
        result.gradient[axis] = 1.0L;
        return result;
    }
};

template <std::size_t N>
[[nodiscard]] constexpr Dual<N> operator+(const Dual<N> &a, const Dual<N> &b) {
    Dual<N> result{a.value + b.value};
    for (std::size_t i = 0; i < N; ++i)
        result.gradient[i] = a.gradient[i] + b.gradient[i];
    return result;
}

template <std::size_t N>
[[nodiscard]] constexpr Dual<N> operator-(const Dual<N> &a, const Dual<N> &b) {
    Dual<N> result{a.value - b.value};
    for (std::size_t i = 0; i < N; ++i)
        result.gradient[i] = a.gradient[i] - b.gradient[i];
    return result;
}

template <std::size_t N>
[[nodiscard]] constexpr Dual<N> operator*(const Dual<N> &a, const Dual<N> &b) {
    Dual<N> result{a.value * b.value};
    for (std::size_t i = 0; i < N; ++i)
        result.gradient[i] = a.gradient[i] * b.value + a.value * b.gradient[i];
    return result;
}

template <std::size_t N>
[[nodiscard]] constexpr Dual<N> operator/(const Dual<N> &a, const Dual<N> &b) {
    Dual<N> result{a.value / b.value};
    for (std::size_t i = 0; i < N; ++i)
        result.gradient[i] = (a.gradient[i] - result.value * b.gradient[i]) / b.value;
    return result;
}

template <std::size_t N> [[nodiscard]] Dual<N> sqrt(const Dual<N> &a) {
    Dual<N> result{std::sqrt(a.value)};
    for (std::size_t i = 0; i < N; ++i)
        result.gradient[i] = a.gradient[i] / (2.0L * result.value);
    return result;
}

} // namespace kerr::math
