#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace kerr::numerics {

template <std::size_t N> using State = std::array<double, N>;

template <std::size_t N> [[nodiscard]] bool finite(const State<N> &state) noexcept {
    for (double value : state)
        if (!std::isfinite(value))
            return false;
    return true;
}

template <std::size_t N> struct Step final {
    bool valid{};
    State<N> end{}, error{}, endpoint_derivative{};
    std::array<State<N>, 4> dense{}; // y(theta)=y0+h*sum dense[j]*theta^(j+1)
    std::size_t evaluations{};
};

// Autonomous Dormand-Prince 5(4), with FSAL supplied by the caller.
// Tableau: Dormand & Prince (1980); quartic extension: Shampine (1986).
// Coefficients cross-checked with Numipedia and SciPy 1.14.1 RK45.
// Dense table adapted from SciPy, copyright 2001-2002 Enthought, Inc.,
// 2003-2024 SciPy Developers. BSD-3-Clause notice: third_party/licenses/scipy-1.14.1.txt.
template <std::size_t N, typename System>
[[nodiscard]] Step<N> dopri5(System &&rhs, const State<N> &initial, const State<N> &first,
                             const double h) {
    constexpr std::array<std::array<double, 7>, 7> a{
        {{},
         {1.0 / 5.0},
         {3.0 / 40.0, 9.0 / 40.0},
         {44.0 / 45.0, -56.0 / 15.0, 32.0 / 9.0},
         {19372.0 / 6561.0, -25360.0 / 2187.0, 64448.0 / 6561.0, -212.0 / 729.0},
         {9017.0 / 3168.0, -355.0 / 33.0, 46732.0 / 5247.0, 49.0 / 176.0, -5103.0 / 18656.0},
         {35.0 / 384.0, 0.0, 500.0 / 1113.0, 125.0 / 192.0, -2187.0 / 6784.0, 11.0 / 84.0}}};
    constexpr State<7> fourth{
        5179.0 / 57600.0, 0.0,       7571.0 / 16695.0, 393.0 / 640.0, -92097.0 / 339200.0,
        187.0 / 2100.0,   1.0 / 40.0};
    // Dense-output mathematical coefficient table; see THIRD_PARTY.md.
    constexpr std::array<std::array<double, 4>, 7> p{
        {{1.0, -8048581381.0 / 2820520608.0, 8663915743.0 / 2820520608.0,
          -12715105075.0 / 11282082432.0},
         {},
         {0.0, 131558114200.0 / 32700410799.0, -68118460800.0 / 10900136933.0,
          87487479700.0 / 32700410799.0},
         {0.0, -1754552775.0 / 470086768.0, 14199869525.0 / 1410260304.0,
          -10690763975.0 / 1880347072.0},
         {0.0, 127303824393.0 / 49829197408.0, -318862633887.0 / 49829197408.0,
          701980252875.0 / 199316789632.0},
         {0.0, -282668133.0 / 205662961.0, 2019193451.0 / 616988883.0, -1453857185.0 / 822651844.0},
         {0.0, 40617522.0 / 29380423.0, -110615467.0 / 29380423.0, 69997945.0 / 29380423.0}}};
    Step<N> result{};
    if (!finite(initial) || !finite(first) || !std::isfinite(h))
        return result;
    std::array<State<N>, 7> k{};
    k[0] = first;
    for (std::size_t stage = 1; stage < 7; ++stage) {
        State<N> trial{};
        for (std::size_t i = 0; i < N; ++i) {
            long double sum{};
            for (std::size_t j = 0; j < stage; ++j)
                sum += static_cast<long double>(a[stage][j]) * static_cast<long double>(k[j][i]);
            trial[i] = static_cast<double>(static_cast<long double>(initial[i]) +
                                           static_cast<long double>(h) * sum);
        }
        ++result.evaluations;
        if (!finite(trial) || !rhs(trial, k[stage]) || !finite(k[stage]))
            return result;
        if (stage == 6)
            result.end = trial;
    }
    result.endpoint_derivative = k[6];
    for (std::size_t i = 0; i < N; ++i) {
        long double error{};
        for (std::size_t j = 0; j < 7; ++j)
            error +=
                static_cast<long double>(a[6][j] - fourth[j]) * static_cast<long double>(k[j][i]);
        result.error[i] = static_cast<double>(static_cast<long double>(h) * error);
        for (std::size_t power = 0; power < 4; ++power) {
            long double coefficient{};
            for (std::size_t j = 0; j < 7; ++j)
                coefficient +=
                    static_cast<long double>(p[j][power]) * static_cast<long double>(k[j][i]);
            result.dense[power][i] = static_cast<double>(coefficient);
        }
    }
    result.valid = finite(result.end) && finite(result.error);
    for (const auto &coefficient : result.dense)
        result.valid = result.valid && finite(coefficient);
    return result;
}

template <std::size_t N>
[[nodiscard]] State<N> dense_state(const State<N> &initial, const Step<N> &step, double h,
                                   double theta) noexcept {
    if (theta == 0.0)
        return initial;
    if (theta == 1.0)
        return step.end;
    State<N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        long double polynomial = static_cast<long double>(step.dense[3][i]);
        for (std::size_t j = 3; j > 0; --j)
            polynomial = static_cast<long double>(step.dense[j - 1][i]) +
                         static_cast<long double>(theta) * polynomial;
        result[i] = static_cast<double>(static_cast<long double>(initial[i]) +
                                        static_cast<long double>(h) *
                                            static_cast<long double>(theta) * polynomial);
    }
    return result;
}

// Independent eighth-order endpoint reference: modified midpoint with even
// substep counts 2,4,6,8 and polynomial extrapolation in (h/n)^2.
// The embedded sixth-order difference is O(h^7). No dense output is provided.
template <std::size_t N, typename System>
[[nodiscard]] Step<N> extrapolated_midpoint8(System &&rhs, const State<N> &initial,
                                             const State<N> &first, double h) {
    Step<N> result{};
    if (!finite(initial) || !finite(first) || !std::isfinite(h))
        return result;
    std::array<std::array<State<N>, 4>, 4> table{};
    for (std::size_t level = 0; level < 4; ++level) {
        const std::size_t substeps = 2 * (level + 1);
        const double dt = h / static_cast<double>(substeps);
        State<N> previous = initial, current{}, derivative{};
        for (std::size_t i = 0; i < N; ++i)
            current[i] = initial[i] + dt * first[i];
        for (std::size_t sub = 1; sub < substeps; ++sub) {
            ++result.evaluations;
            if (!finite(current) || !rhs(current, derivative) || !finite(derivative))
                return result;
            State<N> next{};
            for (std::size_t i = 0; i < N; ++i)
                next[i] = previous[i] + 2.0 * dt * derivative[i];
            previous = current;
            current = next;
        }
        ++result.evaluations;
        if (!finite(current) || !rhs(current, derivative) || !finite(derivative))
            return result;
        for (std::size_t i = 0; i < N; ++i)
            table[level][0][i] = 0.5 * (previous[i] + current[i] + dt * derivative[i]);
        for (std::size_t column = 1; column <= level; ++column) {
            const double ratio =
                static_cast<double>(level + 1) / static_cast<double>(level - column + 1);
            for (std::size_t i = 0; i < N; ++i) {
                const double fine = table[level][column - 1][i];
                table[level][column][i] =
                    fine + (fine - table[level - 1][column - 1][i]) / (ratio * ratio - 1.0);
            }
        }
    }
    result.end = table[3][3];
    for (std::size_t i = 0; i < N; ++i)
        result.error[i] = result.end[i] - table[3][2][i];
    ++result.evaluations;
    result.valid = finite(result.end) && finite(result.error) &&
                   rhs(result.end, result.endpoint_derivative) &&
                   finite(result.endpoint_derivative);
    return result;
}

template <std::size_t N>
[[nodiscard]] double scaled_error(const State<N> &initial, const Step<N> &step,
                                  const State<N> &atol, double rtol) noexcept {
    long double norm{};
    for (std::size_t i = 0; i < N; ++i) {
        const long double scale =
            static_cast<long double>(atol[i]) +
            static_cast<long double>(rtol) *
                static_cast<long double>(std::max(std::abs(initial[i]), std::abs(step.end[i])));
        if (!(scale > 0.0L) || !std::isfinite(scale))
            return std::numeric_limits<double>::infinity();
        const long double component = static_cast<long double>(step.error[i]) / scale;
        norm += component * component;
    }
    return static_cast<double>(std::sqrt(norm / static_cast<long double>(N)));
}

[[nodiscard]] inline double step_factor(double error, double exponent, bool rejected) noexcept {
    if (!std::isfinite(error))
        return 0.2;
    const double factor = error > 0.0 ? 0.9 * std::pow(error, -exponent) : 5.0;
    return std::clamp(factor, 0.2, rejected ? 1.0 : 5.0);
}

} // namespace kerr::numerics

// Cash-Karp embedded RK45

// Dormand-Prince DOP853
