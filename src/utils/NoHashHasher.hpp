#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace rl {
    template <typename T>
    struct NoHashHasher;

    template <typename T>
    requires std::integral<T>
    struct NoHashHasher<T> {
        static constexpr size_t operator()(T key) {
            return static_cast<size_t>(key);
        }
    };
}  // namespace rl
