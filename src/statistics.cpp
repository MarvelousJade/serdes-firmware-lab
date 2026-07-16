#include "serdes/phy_types.hpp"

#include <algorithm>
#include <cmath>

namespace serdes {

double ber_upper_bound_95(const Measurement& measurement) noexcept {
    if (measurement.symbols == 0U) {
        return 1.0;
    }

    const double count = static_cast<double>(measurement.symbols);
    if (measurement.errors == 0U) {
        return std::min(1.0, 3.0 / count);
    }

    // One-sided Wilson score estimate with z = Phi^-1(0.95).
    constexpr double kZ = 1.644'853'626'951'472'2;
    constexpr double kZSquared = kZ * kZ;
    const double observed = static_cast<double>(measurement.errors) / count;
    const double denominator = 1.0 + kZSquared / count;
    const double center = observed + kZSquared / (2.0 * count);
    const double radius = kZ * std::sqrt(
        observed * (1.0 - observed) / count + kZSquared / (4.0 * count * count));
    return std::clamp((center + radius) / denominator, 0.0, 1.0);
}

}  // namespace serdes
