#include "serdes/firmware_controller.hpp"
#include "serdes/phy_driver.hpp"
#include "serdes/prbs31.hpp"
#include "serdes/simulated_phy.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

class TestSuite {
public:
    void check(const bool condition, const std::string_view expression, const int line) {
        ++checks_;
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL line " << line << ": " << expression << '\n';
        }
    }

    [[nodiscard]] int finish() const {
        if (failures_ == 0) {
            std::cout << "PASS: " << checks_ << " checks\n";
            return 0;
        }
        std::cerr << "FAIL: " << failures_ << " of " << checks_ << " checks\n";
        return 1;
    }

private:
    int checks_{0};
    int failures_{0};
};

#define CHECK(suite, expression) (suite).check((expression), #expression, __LINE__)

class NeverCompleteIo final : public serdes::IRegisterIo {
public:
    [[nodiscard]] std::uint32_t read32(const serdes::Register address) const override {
        return address == serdes::Register::Status ? serdes::status_bits::kPllLocked : 0U;
    }
    void write32(serdes::Register, std::uint32_t) override {}
    void tick() override { ++ticks; }

    std::uint32_t ticks{0};
};

void test_prbs31_signature(TestSuite& suite) {
    serdes::Prbs31 prbs{};
    std::uint64_t signature = 0U;
    for (int bit = 0; bit < 64; ++bit) {
        signature = (signature << 1U) | (prbs.next_bit() ? 1U : 0U);
    }
    CHECK(suite, signature == 0xFFFF'FFFE'0000'001CULL);

    prbs.reset(0U);
    CHECK(suite, prbs.state() == serdes::Prbs31::kDefaultSeed);
}

void test_register_encoding_and_clamping(TestSuite& suite) {
    const auto profile = serdes::find_channel_profile("short");
    CHECK(suite, profile.has_value());
    if (!profile.has_value()) {
        return;
    }

    serdes::SimulatedPhy model{*profile};
    serdes::PhyDriver phy{model};
    phy.reset();
    phy.set_ctle_code(99U);
    CHECK(suite, phy.ctle_code() == serdes::kMaxCtleCode);

    phy.set_dfe_tap_code(0U, -17);
    phy.set_dfe_tap_code(1U, 200);
    phy.set_dfe_tap_code(2U, -200);
    CHECK(suite, phy.dfe_tap_code(0U) == -17);
    CHECK(suite, phy.dfe_tap_code(1U) == serdes::kMaxDfeTapCode);
    CHECK(suite, phy.dfe_tap_code(2U) == serdes::kMinDfeTapCode);
}

void test_measurement_timeout(TestSuite& suite) {
    NeverCompleteIo io{};
    serdes::PhyDriver phy{io};
    const auto measurement = phy.measure(1'024U, serdes::MeasurementMode::Training);
    CHECK(suite, !measurement.valid);
    CHECK(suite, io.ticks == 8U);
}

bool same_measurement(const serdes::Measurement& first, const serdes::Measurement& second) {
    return first.valid == second.valid && first.errors == second.errors &&
           first.symbols == second.symbols &&
           first.mean_squared_error == second.mean_squared_error &&
           first.mean_margin == second.mean_margin &&
           first.error_correlations == second.error_correlations;
}

void test_sequence_replay(TestSuite& suite) {
    constexpr std::array<std::uint32_t, 4> seeds{0U, 1U, 42U, 0xFFFF'FFFFU};
    for (const auto profile_name : {"short", "medium", "long"}) {
        serdes::SimulatedPhy model{*serdes::find_channel_profile(profile_name)};
        serdes::PhyDriver phy{model};
        phy.reset();
        for (std::uint32_t tick = 0; tick < model.channel_profile().pll_lock_delay_ticks; ++tick) {
            phy.tick();
        }

        for (const auto mode : {serdes::MeasurementMode::Training,
                               serdes::MeasurementMode::Verification}) {
            for (const auto seed : seeds) {
                // An odd window leaves a cached noise sample, which restart must discard.
                phy.restart_test_sequence(seed);
                const auto first = phy.measure(1'025U, mode);
                phy.restart_test_sequence(seed);
                const auto repeated = phy.measure(1'025U, mode);
                CHECK(suite, first.valid && repeated.valid &&
                                 std::isfinite(first.mean_squared_error) &&
                                 std::isfinite(first.mean_margin));
                CHECK(suite, same_measurement(first, repeated));

                if (seed == 0U) {
                    phy.restart_test_sequence(serdes::Prbs31::kDefaultSeed);
                    CHECK(suite, same_measurement(first, phy.measure(1'025U, mode)));
                }
            }
        }
    }
}

void test_ber_upper_estimate(TestSuite& suite) {
    serdes::Measurement zero_errors{};
    zero_errors.symbols = 500'000U;
    zero_errors.valid = true;
    CHECK(suite, std::abs(serdes::ber_upper_bound_95(zero_errors) - 6.0e-6) < 1.0e-12);
    zero_errors.symbols = 1U;
    CHECK(suite, serdes::ber_upper_bound_95(zero_errors) == 1.0);

    serdes::Measurement observed_errors{};
    observed_errors.errors = 13U;
    observed_errors.symbols = 500'000U;
    observed_errors.valid = true;
    CHECK(suite, serdes::ber_upper_bound_95(observed_errors) > observed_errors.ber());
    CHECK(suite, serdes::ber_upper_bound_95(observed_errors) < 5.0e-5);
}

void test_bringup(TestSuite& suite, const std::string_view profile_name) {
    const auto profile = serdes::find_channel_profile(profile_name);
    CHECK(suite, profile.has_value());
    if (!profile.has_value()) {
        return;
    }

    serdes::SimulatedPhy model{*profile};
    serdes::PhyDriver phy{model};
    serdes::FirmwareConfig config{};
    config.verify_symbols = 100'000U;
    config.sweep_symbols = 8'192U;
    serdes::FirmwareController firmware{phy, config};
    const auto report = firmware.bring_up(42U);

    CHECK(suite, report.success);
    CHECK(suite, report.fault == serdes::FaultReason::None);
    CHECK(suite, report.baseline.valid);
    CHECK(suite, report.trained.valid);
    CHECK(suite, report.trained.ber() <= config.maximum_ber);
    CHECK(suite, report.trained.ber() <= report.baseline.ber());
    if (report.baseline.errors != 0U) {
        CHECK(suite, report.trained.ber() < report.baseline.ber());
    }
    CHECK(suite, report.state_trace[report.state_trace_size - 1U] == serdes::LinkState::LinkUp);
}

void test_pll_timeout(TestSuite& suite) {
    auto profile = *serdes::find_channel_profile("short");
    profile.force_pll_fault = true;
    serdes::SimulatedPhy model{profile};
    serdes::PhyDriver phy{model};
    serdes::FirmwareConfig config{};
    config.pll_timeout_ticks = 3U;
    serdes::FirmwareController firmware{phy, config};
    const auto report = firmware.bring_up(7U);

    CHECK(suite, !report.success);
    CHECK(suite, report.fault == serdes::FaultReason::PllTimeout);
    CHECK(suite, firmware.state() == serdes::LinkState::Fault);
}

void test_training_exhaustion(TestSuite& suite) {
    const auto profile = *serdes::find_channel_profile("medium");
    serdes::SimulatedPhy model{profile};
    serdes::PhyDriver phy{model};
    serdes::FirmwareConfig config{};
    config.max_training_windows = 1U;
    config.stable_training_windows = 3U;
    serdes::FirmwareController firmware{phy, config};
    const auto report = firmware.bring_up(17U);

    CHECK(suite, !report.success);
    CHECK(suite, report.fault == serdes::FaultReason::TrainingNotConverged);
    CHECK(suite, firmware.state() == serdes::LinkState::Fault);
}

void test_degradation_hysteresis(TestSuite& suite) {
    const auto short_profile = *serdes::find_channel_profile("short");
    serdes::SimulatedPhy model{short_profile};
    serdes::PhyDriver phy{model};
    serdes::FirmwareConfig config{};
    config.verify_symbols = 80'000U;
    config.degraded_windows_before_retrain = 3U;
    serdes::FirmwareController firmware{phy, config};
    const auto report = firmware.bring_up(99U);
    CHECK(suite, report.success);

    auto degraded = *serdes::find_channel_profile("long");
    degraded.impulse_response = {1.0, 1.15, -0.82, 0.48};
    degraded.noise_sigma = 0.28;
    model.set_channel_profile(degraded);

    CHECK(suite, firmware.check_link_health(30'000U, 100U) ==
                     serdes::HealthAction::Observe);
    CHECK(suite, firmware.check_link_health(30'000U, 101U) ==
                     serdes::HealthAction::Observe);
    CHECK(suite, firmware.check_link_health(30'000U, 102U) ==
                     serdes::HealthAction::RetrainRequired);
    CHECK(suite, firmware.state() == serdes::LinkState::Degraded);
}

}  // namespace

int main() {
    TestSuite suite{};
    test_prbs31_signature(suite);
    test_register_encoding_and_clamping(suite);
    test_measurement_timeout(suite);
    test_sequence_replay(suite);
    test_ber_upper_estimate(suite);
    test_bringup(suite, "short");
    test_bringup(suite, "medium");
    test_bringup(suite, "long");
    test_pll_timeout(suite);
    test_training_exhaustion(suite);
    test_degradation_hysteresis(suite);
    return suite.finish();
}
