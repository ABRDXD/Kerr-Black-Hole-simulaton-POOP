#include "kerr/physics/Units.hpp"

#include <cmath>
#include <stdexcept>

namespace kerr::physics {

PhysicalScale::PhysicalScale(const long double solar_masses)
    : solar_masses_{solar_masses} {
    if (!std::isfinite(solar_masses) || !(solar_masses > 0.0L)) {
        throw std::invalid_argument{"Physical black-hole mass must be finite and positive"};
    }

    mass_kilograms_ = solar_masses_ * conventional_solar_mass_kilograms;
    const long double c_squared = speed_of_light_metres_per_second *
                                  speed_of_light_metres_per_second;
    length_metres_per_m_ = gravitational_constant_si * mass_kilograms_ / c_squared;
    time_seconds_per_m_ = length_metres_per_m_ / speed_of_light_metres_per_second;
}

} // namespace kerr::physics

