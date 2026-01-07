#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <random>

namespace vesper {

/// @brief Unique identifier for assets based on path hash
/// Uses FNV-1a 64-bit hash for stable, path-independent asset references
struct AssetID
{
    uint64_t hash{0};

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr AssetID() = default;
    constexpr explicit AssetID(uint64_t h) : hash(h) {}

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /// @brief Create AssetID from normalized path
    /// @param path Asset path (will be normalized internally)
    static AssetID fromPath(const std::string& path)
    {
        return AssetID(fnv1a64(normalizePath(path)));
    }

    /// @brief Generate random AssetID for runtime-created resources
    static AssetID generate()
    {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
        return AssetID(dist(gen));
    }

    /// @brief Create invalid AssetID
    static constexpr AssetID invalid() { return AssetID{0}; }

    // =========================================================================
    // Validation
    // =========================================================================

    [[nodiscard]] constexpr bool isValid() const { return hash != 0; }
    constexpr explicit operator bool() const { return isValid(); }

    // =========================================================================
    // Comparison
    // =========================================================================

    constexpr bool operator==(const AssetID& other) const { return hash == other.hash; }
    constexpr bool operator!=(const AssetID& other) const { return hash != other.hash; }
    constexpr bool operator<(const AssetID& other) const { return hash < other.hash; }

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Get raw hash value for serialization
    [[nodiscard]] constexpr uint64_t value() const { return hash; }

private:
    /// @brief FNV-1a 64-bit hash
    static constexpr uint64_t fnv1a64(const std::string& str)
    {
        constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;

        uint64_t hash = FNV_OFFSET;
        for (char c : str)
        {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME;
        }
        return hash;
    }

    /// @brief Normalize path (convert backslashes, lowercase, remove trailing slash)
    static std::string normalizePath(const std::string& path)
    {
        std::string result;
        result.reserve(path.size());

        for (char c : path)
        {
            if (c == '\\')
            {
                result += '/';
            }
            else if (c >= 'A' && c <= 'Z')
            {
                result += static_cast<char>(c + ('a' - 'A'));
            }
            else
            {
                result += c;
            }
        }

        // Remove trailing slash
        while (!result.empty() && result.back() == '/')
        {
            result.pop_back();
        }

        return result;
    }
};

} // namespace vesper

// Specialization of std::hash for use in unordered containers
namespace std {

template<>
struct hash<vesper::AssetID>
{
    size_t operator()(const vesper::AssetID& id) const noexcept
    {
        return static_cast<size_t>(id.hash);
    }
};

} // namespace std
