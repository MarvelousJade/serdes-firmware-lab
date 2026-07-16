#pragma once

#include "serdes/phy_types.hpp"
#include "serdes/register_map.hpp"

#include <cstddef>
#include <cstdint>

namespace serdes {

class PhyDriver {
public:
    explicit PhyDriver(IRegisterIo& io) : io_(io) {}

    void reset();
    void tick();
    [[nodiscard]] bool pll_locked() const;

    void restart_pattern(std::uint32_t seed);
    void set_ctle_code(std::uint8_t code);
    [[nodiscard]] std::uint8_t ctle_code() const;

    void set_dfe_tap_code(std::size_t index, int code);
    [[nodiscard]] std::int8_t dfe_tap_code(std::size_t index) const;
    void clear_dfe_taps();
    [[nodiscard]] DfeTapCodes dfe_tap_codes() const;

    [[nodiscard]] Measurement measure(std::uint32_t symbols, bool training_mode);

private:
    IRegisterIo& io_;
};

}  // namespace serdes

