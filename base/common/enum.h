#pragma once

#include <cstdint>
#include <type_traits>

constexpr uint32_t makeBitmaskBit(uint32_t bit) noexcept {
    return uint32_t{1} << bit;
}

template <typename Enum> constexpr bool testBitmaskContain(Enum value, Enum flag) noexcept {
    static_assert(std::is_enum_v<Enum>);

    using Underlying = std::underlying_type_t<Enum>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flag)) == static_cast<Underlying>(flag);
}

#define ENABLE_BITMASK_OPERATION(Enum)                                                                                 \
    constexpr Enum operator|(Enum lhs, Enum rhs) noexcept {                                                            \
        using Underlying = std::underlying_type_t<Enum>;                                                               \
        return static_cast<Enum>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));                         \
    }                                                                                                                  \
    constexpr Enum operator&(Enum lhs, Enum rhs) noexcept {                                                            \
        using Underlying = std::underlying_type_t<Enum>;                                                               \
        return static_cast<Enum>(static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));                         \
    }                                                                                                                  \
    constexpr Enum operator^(Enum lhs, Enum rhs) noexcept {                                                            \
        using Underlying = std::underlying_type_t<Enum>;                                                               \
        return static_cast<Enum>(static_cast<Underlying>(lhs) ^ static_cast<Underlying>(rhs));                         \
    }                                                                                                                  \
    constexpr Enum operator~(Enum value) noexcept {                                                                    \
        using Underlying = std::underlying_type_t<Enum>;                                                               \
        return static_cast<Enum>(~static_cast<Underlying>(value));                                                     \
    }                                                                                                                  \
    constexpr Enum& operator|=(Enum& lhs, Enum rhs) noexcept {                                                         \
        return lhs = lhs | rhs;                                                                                        \
    }                                                                                                                  \
    constexpr Enum& operator&=(Enum& lhs, Enum rhs) noexcept {                                                         \
        return lhs = lhs & rhs;                                                                                        \
    }                                                                                                                  \
    constexpr Enum& operator^=(Enum& lhs, Enum rhs) noexcept {                                                         \
        return lhs = lhs ^ rhs;                                                                                        \
    }
