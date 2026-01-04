#pragma once

#include <cstddef>
#include <functional>

namespace vesper {

// Hash combine utility (based on boost::hash_combine)
template<typename T>
inline void hash_combine(std::size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Variadic hash combine
template<typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& value, const Rest&... rest) {
    hash_combine(seed, value);
    hash_combine(seed, rest...);
}

} // namespace vesper