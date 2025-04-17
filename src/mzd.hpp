#include <zstd.h>
#include <vector>
#include <cstdint>
#include <algorithm>

using byte_t = std::uint8_t;
using buffer_t = std::vector<byte_t>;

namespace mzd
{
    /// @brief Implementation details of byte-shuffling and delta codec
    namespace inner {
        template <typename T>
        void delta_encode(std::vector<T> &data)
        {
            size_t n = data.size();
            if (n < 2)
            {
                return;
            }
            T prev = data[0];
            T offset = data[0];

            for (size_t i = 1; i < n; i++)
            {
                T tmp = data[i];
                data[i] += offset - prev;
                prev = tmp;
            }
        }

        template <typename T>
        void delta_decode(std::vector<T> &data)
        {
            size_t n = data.size();
            if (n < 2)
            {
                return;
            }

            T offset = data[0];
            T prev = data[1];
            for (size_t i = 2; i < n; i++)
            {
                data[i] += prev - offset;
                prev = data[i];
            }
        }

        template <typename T>
        void transpose(const std::vector<T> &data, buffer_t &buffer)
        {
            auto nData = data.size();
            auto nBytes = nData * sizeof(T);

            buffer.clear();
            buffer.reserve(nBytes);
            for (size_t i = 0; i < sizeof(T); i++)
            {
                for (size_t j = 0; j < data.size(); j++)
                {
                    auto value = data[j];
                    auto byteView = reinterpret_cast<uint8_t *>(&value);
                    buffer.push_back(byteView[i]);
                }
            }
            return;
        }

        template <typename T>
        void reverse_tranpose(const buffer_t &buffer, std::vector<T> &data)
        {
            auto nBytes = buffer.size();
            auto nData = nBytes / sizeof(T);
            data.resize(nData);
            for (size_t i = 0; i < buffer.size(); i++)
            {
                auto datum = &data[i % nData];
                auto byteView = reinterpret_cast<uint8_t *>(datum);
                byteView[i / nData] = buffer[i];
            }
            return;
        }

    }


    using namespace inner;

    /// @brief Compress an array of numerical data using byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param data The data array to compress
    /// @param transposeBuffer An intermediate byte buffer to hold the shuffled bytes into
    /// @param outBuffer A byte buffer to write ZSTD-compressed bytes to
    /// @param level The ZSTD compression level
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t compress_buffer(const std::vector<T> &data,
                           buffer_t &transposeBuffer,
                           buffer_t &outBuffer,
                           int level = ZSTD_defaultCLevel())
    {
        transposeBuffer.clear();
        transpose(data, transposeBuffer);
        auto outputBound = ZSTD_compressBound(transposeBuffer.size());
        outBuffer.resize(outputBound);
        auto used = ZSTD_compress(
            (void *)outBuffer.data(),
            outputBound,
            (void *)transposeBuffer.data(),
            transposeBuffer.size(),
            level
        );
        if (ZSTD_isError(used))
        {
            return used;
        }
        outBuffer.resize(used);
        return 0;
    }

    /// @brief Decompress an array of numerical data using byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param buffer A byte buffer to containing ZSTD-compressed bytes
    /// @param transposeBuffer An intermediate byte buffer to hold the shuffled bytes into
    /// @param dataBuffer The data array to decompress into
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t decompress_buffer(const buffer_t &buffer,
                             buffer_t &transposeBuffer,
                             std::vector<T> &dataBuffer)
    {
        transposeBuffer.clear();
        auto outputBound = ZSTD_getFrameContentSize(buffer.data(), buffer.size());
        if (ZSTD_isError(outputBound))
        {
            return outputBound;
        }
        std::cout << "Output upper bound size is "
                  << outputBound
                  << " bytes long" << std::endl;
        transposeBuffer.resize(outputBound);
        auto used = ZSTD_decompress(
            (void *)transposeBuffer.data(),
            outputBound,
            (void *)buffer.data(),
            buffer.size());
        if (ZSTD_isError(used))
        {
            return used;
        }
        transposeBuffer.resize(used);
        reverse_tranpose(transposeBuffer, dataBuffer);
        return 0;
    }

    /// @brief Compress an array of numerical data using delta encoding, byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param data The data array to compress. *This array will be mutated with delta encoding*
    /// @param transposeBuffer An intermediate byte buffer to hold the shuffled bytes into
    /// @param outBuffer A byte buffer to write ZSTD-compressed bytes to
    /// @param level The ZSTD compression level
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t delta_compress_buffer(
        std::vector<T> &data,
        buffer_t &transposeBuffer,
        buffer_t &outBuffer,
        int level = ZSTD_defaultCLevel()
    )
    {
        delta_encode(data);
        return compress_buffer(data, transposeBuffer, outBuffer);
    }

    /// @brief Decompress an array of numerical data using delta encoding, byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param buffer A byte buffer to containing ZSTD-compressed bytes
    /// @param transposeBuffer An intermediate byte buffer to hold the shuffled bytes into
    /// @param dataBuffer The data array to decompress into
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t delta_decompress_buffer(
        const buffer_t &buffer,
        buffer_t &transposeBuffer,
        std::vector<T> &dataBuffer
    )
    {
        auto z = decompress_buffer(buffer, transposeBuffer, dataBuffer);
        delta_decode(dataBuffer);
        return z;
    }
}