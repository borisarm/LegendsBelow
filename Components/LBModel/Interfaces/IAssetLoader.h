#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Generic asset loader interface for binary asset files
    /// Abstraction for file I/O operations needed by domain repositories
    /// </summary>
    class IAssetLoader {
    public:
        virtual ~IAssetLoader() = default;

        /// <summary>
        /// Load binary data from file
        /// </summary>
        virtual std::optional<std::vector<uint8_t>> LoadBinary(const String& filePath) const = 0;

        /// <summary>
        /// Load text data from file
        /// </summary>
        virtual std::optional<String> LoadText(const String& filePath) const = 0;

        /// <summary>
        /// Check if file exists and is readable
        /// </summary>
        virtual bool FileExists(const String& filePath) const = 0;

        /// <summary>
        /// Get file modification time (for hot-reload detection)
        /// </summary>
        virtual std::optional<uint64_t> GetFileTimestamp(const String& filePath) const = 0;

        /// <summary>
        /// Get file size in bytes
        /// </summary>
        virtual std::optional<size_t> GetFileSize(const String& filePath) const = 0;
    };

} // namespace Domain
} // namespace LB