#include "serdes/firmware_controller.hpp"
#include "serdes/simulated_phy.hpp"

#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

std::string_view action_name(const serdes::HealthAction action) {
    switch (action) {
    case serdes::HealthAction::Healthy: return "Healthy";
    case serdes::HealthAction::Observe: return "Observe";
    case serdes::HealthAction::RetrainRequired: return "RetrainRequired";
    case serdes::HealthAction::NotLinkUp: return "NotLinkUp";
    }
    return "Unknown";
}

bool health() {
    const auto good = *serdes::find_channel_profile("short");
    serdes::SimulatedPhy model{good};
    serdes::PhyDriver phy{model};
    serdes::FirmwareController firmware{phy};
    if (!firmware.bring_up(99U).success) {
        std::cerr << "fixture bring-up failed\n";
        return false;
    }
    auto bad = *serdes::find_channel_profile("long");
    bad.impulse_response = {1.0, 1.15, -0.82, 0.48};
    bad.noise_sigma = 0.28;
    constexpr serdes::HealthAction expected[]{
        serdes::HealthAction::Observe, serdes::HealthAction::Observe,
        serdes::HealthAction::Healthy, serdes::HealthAction::Observe,
        serdes::HealthAction::Observe, serdes::HealthAction::RetrainRequired};
    bool passed = true;
    for (unsigned index = 0; index < 6U; ++index) {
        model.set_channel_profile(index == 2U ? good : bad);
        const auto action = firmware.check_link_health(30'000U, 100U + index);
        std::cout << "window " << index + 1U << ": expected " << action_name(expected[index])
                  << ", observed " << action_name(action) << '\n';
        passed = passed && action == expected[index];
    }
    return passed;
}

bool replay() {
    serdes::SimulatedPhy model{*serdes::find_channel_profile("short")};
    serdes::PhyDriver phy{model};
    phy.reset();
    for (unsigned tick = 0; tick < 4U; ++tick) {
        phy.tick();
    }
    phy.restart_test_sequence(42U);
    const auto first = phy.measure(1'025U, serdes::MeasurementMode::Verification);
    phy.restart_test_sequence(42U);
    const auto repeated = phy.measure(1'025U, serdes::MeasurementMode::Verification);
    const bool same = first.valid && repeated.valid && first.errors == repeated.errors &&
        first.symbols == repeated.symbols &&
        first.mean_squared_error == repeated.mean_squared_error &&
        first.mean_margin == repeated.mean_margin &&
        first.error_correlations == repeated.error_correlations;
    std::cout << std::setprecision(12)
              << "first: valid=" << first.valid << ", symbols=" << first.symbols
              << ", errors=" << first.errors << ", mse=" << first.mean_squared_error
              << ", margin=" << first.mean_margin << '\n'
              << "repeat: valid=" << repeated.valid << ", symbols=" << repeated.symbols
              << ", errors=" << repeated.errors << ", mse=" << repeated.mean_squared_error
              << ", margin=" << repeated.mean_margin << '\n'
              << "identical measurements: " << (same ? "yes" : "no") << '\n';
    return same;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || (std::string_view{argv[1]} != "health" &&
                      std::string_view{argv[1]} != "replay")) {
        std::cerr << "Usage: serdes_practice health|replay\n";
        return 2;
    }
    return (std::string_view{argv[1]} == "health" ? health() : replay()) ? 0 : 1;
}
