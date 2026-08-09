#pragma once

namespace kerr::physics {

inline constexpr long double speed_of_light_metres_per_second = 299'792'458.0L;
inline constexpr long double gravitational_constant_si = 6.67430e-11L;
inline constexpr long double conventional_solar_mass_kilograms = 1.98847e30L;

class PhysicalScale final {
public:
    explicit PhysicalScale(long double solar_masses);

    [[nodiscard]] constexpr long double solar_masses() const noexcept {
        return solar_masses_;
    }
    [[nodiscard]] constexpr long double mass_kilograms() const noexcept {
        return mass_kilograms_;
    }
    [[nodiscard]] constexpr long double length_metres_per_m() const noexcept {
        return length_metres_per_m_;
    }
    [[nodiscard]] constexpr long double time_seconds_per_m() const noexcept {
        return time_seconds_per_m_;
    }
    [[nodiscard]] constexpr long double distance_metres(
        const long double radius_over_m) const noexcept {
        return radius_over_m * length_metres_per_m_;
    }
    [[nodiscard]] constexpr long double duration_seconds(
        const long double time_over_m) const noexcept {
        return time_over_m * time_seconds_per_m_;
    }

private:
    long double solar_masses_{};
    long double mass_kilograms_{};
    long double length_metres_per_m_{};
    long double time_seconds_per_m_{};
};

} // namespace kerr::physics

