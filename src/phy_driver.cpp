#include "serdes/phy_driver.hpp"

#include "serdes/prbs31.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace serdes {
namespace {

constexpr double kQ16Scale = 65'536.0;

[[nodiscard]] std::int32_t decode_signed_word(const std::uint32_t value) noexcept {
    return std::bit_cast<std::int32_t>(value);
}

}  // namespace

void PhyDriver::reset() {
    io_.write32(Register::Control, control_bits::kReset);
    io_.write32(Register::Control, 0U);
}

void PhyDriver::tick() {
    io_.tick();
}

bool PhyDriver::pll_locked() const {
    return (io_.read32(Register::Status) & status_bits::kPllLocked) != 0U;
}

void PhyDriver::restart_pattern(const std::uint32_t seed) {
    io_.write32(Register::PatternSeed, seed & Prbs31::kMask);
}

void PhyDriver::set_ctle_code(const std::uint8_t code) {
    io_.write32(Register::CtleCode, std::min(code, kMaxCtleCode));
}

std::uint8_t PhyDriver::ctle_code() const {
    return static_cast<std::uint8_t>(io_.read32(Register::CtleCode) & 0xFFU);
}

void PhyDriver::set_dfe_tap_code(const std::size_t index, const int code) {
    if (index >= kDfeTapCount) {
        return;
    }

    const auto clamped = static_cast<std::int8_t>(
        std::clamp(code, static_cast<int>(kMinDfeTapCode), static_cast<int>(kMaxDfeTapCode)));
    const auto encoded = std::bit_cast<std::uint8_t>(clamped);
    io_.write32(kDfeTapRegisters[index], encoded);
}

std::int8_t PhyDriver::dfe_tap_code(const std::size_t index) const {
    if (index >= kDfeTapCount) {
        return 0;
    }
    const auto encoded = static_cast<std::uint8_t>(io_.read32(kDfeTapRegisters[index]) & 0xFFU);
    return std::bit_cast<std::int8_t>(encoded);
}

void PhyDriver::clear_dfe_taps() {
    for (std::size_t index = 0; index < kDfeTapCount; ++index) {
        set_dfe_tap_code(index, 0);
    }
}

DfeTapCodes PhyDriver::dfe_tap_codes() const {
    DfeTapCodes result{};
    for (std::size_t index = 0; index < kDfeTapCount; ++index) {
        result[index] = dfe_tap_code(index);
    }
    return result;
}

Measurement PhyDriver::measure(const std::uint32_t symbols, const bool training_mode) {
    io_.write32(Register::MeasureSymbols, symbols);
    const auto command = measure_bits::kStart |
                         (training_mode ? measure_bits::kTrainingMode : 0U);
    io_.write32(Register::MeasureControl, command);

    constexpr std::uint32_t kCompletionTimeoutTicks = 8;
    std::uint32_t status = io_.read32(Register::Status);
    for (std::uint32_t tick = 0; tick < kCompletionTimeoutTicks &&
                                 (status & status_bits::kMeasurementDone) == 0U;
         ++tick) {
        io_.tick();
        status = io_.read32(Register::Status);
    }

    Measurement result{};
    if ((status & status_bits::kMeasurementDone) == 0U ||
        (status & status_bits::kMeasurementFault) != 0U) {
        return result;
    }

    result.errors = io_.read32(Register::ErrorCount);
    result.symbols = io_.read32(Register::SymbolCount);
    result.mean_squared_error =
        static_cast<double>(io_.read32(Register::MeanSquaredErrorQ16)) / kQ16Scale;
    result.mean_margin =
        static_cast<double>(decode_signed_word(io_.read32(Register::MeanMarginQ16))) / kQ16Scale;
    for (std::size_t index = 0; index < kDfeTapCount; ++index) {
        result.error_correlations[index] =
            decode_signed_word(io_.read32(kCorrelationRegisters[index]));
    }
    result.valid = result.symbols == symbols && symbols != 0U;
    return result;
}

}  // namespace serdes
