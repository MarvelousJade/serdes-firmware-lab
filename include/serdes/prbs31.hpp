#pragma once

#include <cstdint>

namespace serdes {

class Prbs31 {
public:
    static constexpr std::uint32_t kMask = 0x7FFF'FFFFU;
    static constexpr std::uint32_t kDefaultSeed = kMask;

    explicit Prbs31(std::uint32_t seed = kDefaultSeed);

    void reset(std::uint32_t seed = kDefaultSeed);
    [[nodiscard]] bool next_bit() noexcept;
    [[nodiscard]] int next_nrz_symbol() noexcept;
    [[nodiscard]] std::uint32_t state() const noexcept { return state_; }

private:
    std::uint32_t state_{kDefaultSeed};
};

}  // namespace serdes

