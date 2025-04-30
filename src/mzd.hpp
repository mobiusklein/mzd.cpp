#include <zstd.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <algorithm>
#include <bit>

using byte_t = std::uint8_t;
using buffer_t = std::vector<byte_t>;

namespace mzd
{
    /// @brief Implementation of endianness
    namespace binary
    {
        constexpr bool is_big_endian(void)
        {
            return std::endian::native == std::endian::big;
        }

        template <typename Z>
        union _byte_view_impl
        {
            Z value;
            std::array<uint8_t, sizeof(Z)> view;
        };

        template <typename Z>
        struct byte_view
        {
            _byte_view_impl<Z> inner;

            byte_view() {}

            byte_view(Z value)
            {
                this->inner.value = value;
            }

            Z value() {
                return this->inner.value;
            }

            std::array<uint8_t, sizeof(Z)>& buffer() {
                return this->inner.view;
            }

            byte_view(std::array<uint8_t, sizeof(Z)> view)
            {
                this->inner.view = view;
            }

            static byte_view as_little_endian(Z value)
            {
                auto view = byte_view(value);
                if constexpr
                    (is_big_endian())
                    {
                        view.byteswap();
                    }
                return view;
            }

            static byte_view as_little_endian(std::array<uint8_t, sizeof(Z)> value)
            {
                auto view = byte_view(value);
                if constexpr
                    (is_big_endian())
                    {
                        view.byteswap();
                    }
                return view;
            }

            auto begin()
            {
                return this->inner.view.begin();
            }

            auto end()
            {
                return this->inner.view.end();
            }

            int byteswap()
            {
                std::reverse(this->begin(), this->end());
                return 0;
            }
        };
    }

    /// @brief Implementation details of byte-shuffling and delta codec
    namespace inner
    {
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

    /// @brief Implementation of the dictionary codec
    namespace dict
    {

        using mzd::binary::byte_view;
        using mzd::binary::is_big_endian;

        template <typename T, typename I, typename K>
        int encode_dictionary_indices(const std::vector<T> &data, buffer_t &outBuffer, std::vector<I> sorted_values)
        {
            std::unordered_map<I, size_t> value_to_indices;
            value_to_indices.reserve(sorted_values.size());

            for (auto i = 0; i < sorted_values.size(); i++)
            {
                value_to_indices[sorted_values[i]] = i;
            }

            uint64_t n_values = sorted_values.size();

            auto offset_to_data = (sizeof(I) * n_values) + (sizeof(uint64_t) * 2);
            auto view = byte_view<uint64_t>::as_little_endian(offset_to_data);
            outBuffer.insert(outBuffer.end(), view.begin(), view.end());

            view = byte_view<uint64_t>::as_little_endian(n_values);
            outBuffer.insert(outBuffer.end(), view.begin(), view.end());

            for (auto vi : sorted_values)
            {
                byte_view<I> view = byte_view<I>::as_little_endian(vi);
                outBuffer.insert(outBuffer.end(), view.begin(), view.end());
            }

            for (auto val : data)
            {
                I bytes_of = *reinterpret_cast<I *>(&val);
                size_t idx = value_to_indices[bytes_of];
                K k_idx = *reinterpret_cast<K *>(&idx);
                byte_view<K> view = byte_view<K>::as_little_endian(k_idx);
                outBuffer.insert(outBuffer.end(), view.begin(), view.end());
            }

            return outBuffer.size();
        }

        template <typename T, typename I>
        int encode_values(const std::vector<T> &data, buffer_t &outBuffer)
        {
            std::unordered_set<I> value_codes;
            for (T val : data)
            {
                I bytes_of = *reinterpret_cast<I *>(&val);
                value_codes.insert(bytes_of);
            }

            std::vector<I> value_codes_sorted = std::vector(value_codes.cbegin(), value_codes.cend());
            std::sort(value_codes_sorted.begin(), value_codes_sorted.end());

            auto n_values = value_codes_sorted.size();

            if (n_values <= pow(2, 8))
            {
                return encode_dictionary_indices<T, I, uint8_t>(data, outBuffer, value_codes_sorted);
            }
            else if (n_values <= pow(2, 16))
            {
                return encode_dictionary_indices<T, I, uint16_t>(data, outBuffer, value_codes_sorted);
            }
            else if (n_values <= pow(2, 32))
            {
                return encode_dictionary_indices<T, I, uint32_t>(data, outBuffer, value_codes_sorted);
            }
            else if (n_values <= pow(2, 64))
            {
                return encode_dictionary_indices<T, I, uint64_t>(data, outBuffer, value_codes_sorted);
            }
            else
            {
                throw new std::exception("Cannot encode a dictionary with more than 2 ** 64 distinct values");
            }
            return 0;
        }

        template <typename T>
        int dictionary_encode(const std::vector<T> &data, buffer_t &outBuffer)
        {
            if (sizeof(T) <= 1)
            {
                return encode_values<T, uint8_t>(data, outBuffer);
            }
            else if (sizeof(T) <= 2)
            {
                return encode_values<T, uint16_t>(data, outBuffer);
            }
            else if (sizeof(T) <= 4)
            {
                return encode_values<T, uint32_t>(data, outBuffer);
            }
            else if (sizeof(T) <= 8)
            {
                return encode_values<T, uint64_t>(data, outBuffer);
            }
            else
            {
                throw new std::exception("Cannot encode a dictionary with more values longer than 8 bytes");
            }
            return 0;
        }

        template <typename T, typename I>
        int decode_values(const buffer_t &data, size_t offset, size_t n_values, std::vector<T> &values)
        {
            auto start = data.begin() + 16;
            auto end = data.begin() + offset;
            auto step_size = sizeof(I);
            byte_view<I> block;

            auto i = 0;
            while (start != end)
            {
                std::copy(start, start + step_size, block.begin());
                if constexpr (is_big_endian()) {
                    block.byteswap();
                }
                T val = *reinterpret_cast<T *>(&block.buffer());
                values.push_back(val);
                start += step_size;
                i += 1;
            }
            return i;
        }

        template <typename T, typename K>
        int decode_indices(const buffer_t &data, size_t offset, std::vector<T> &values_lookup, std::vector<T> &values)
        {
            auto start = data.begin() + offset;
            auto end = data.end();

            auto step_size = sizeof(K);
            byte_view<K> block;

            while (start != end)
            {
                std::copy(start, start + step_size, block.begin());
                if constexpr
                    (is_big_endian())
                    {
                        block.byteswap();
                    }
                K idx = *reinterpret_cast<K *>(&block);
                T val = values_lookup[idx];
                values.push_back(val);
                start += step_size;
            }
            return 0;
        }

        /// @brief Decode a dictionary-compressed byte buffer
        /// @tparam T The type being decoded
        /// @param data The dictionary-encoded data buffer
        /// @param outBuffer The buffer to decode elements into
        /// @return 0 if successful, otherwise an error
        template <typename T>
        int dictionary_decode(const buffer_t &data, std::vector<T> &outBuffer)
        {
            if (data.size() < 16)
            {
                throw new std::exception("Buffer less than 16 bytes long, invalid dictionary buffer");
            }

            byte_view<uint64_t> view;
            std::copy(data.begin(), data.begin() + 8, view.begin());
            auto offset = view.value();

            std::copy(data.begin() + 8, data.begin() + 16, view.begin());
            auto n_values = view.value();

            if (data.size() < offset)
            {
                throw new std::exception("Buffer less than value offsets, invalid dictionary buffer");
            }

            auto value_size = (offset - 16) / n_values;

            std::vector<T> value_lookup;
            if (value_size == 1)
            {
                decode_values<T, uint8_t>(data, offset, n_values, value_lookup);
                if (n_values < pow(2, 8))
                {
                    decode_indices<T, uint8_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 16))
                {
                    decode_indices<T, uint16_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 32))
                {
                    decode_indices<T, uint32_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 64))
                {
                    decode_indices<T, uint64_t>(data, offset, value_lookup, outBuffer);
                }
                else
                {
                    throw new std::exception("Too many value indices!");
                }
            }
            else if (value_size == 2)
            {
                decode_values<T, uint16_t>(data, offset, n_values, value_lookup);
                if (n_values < pow(2, 8))
                {
                    decode_indices<T, uint8_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 16))
                {
                    decode_indices<T, uint16_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 32))
                {
                    decode_indices<T, uint32_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 64))
                {
                    decode_indices<T, uint64_t>(data, offset, value_lookup, outBuffer);
                }
                else
                {
                    throw new std::exception("Too many value indices!");
                }
            }
            else if (value_size == 4)
            {
                decode_values<T, uint32_t>(data, offset, n_values, value_lookup);
                if (n_values < pow(2, 8))
                {
                    decode_indices<T, uint8_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 16))
                {
                    decode_indices<T, uint16_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 32))
                {
                    decode_indices<T, uint32_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 64))
                {
                    decode_indices<T, uint64_t>(data, offset, value_lookup, outBuffer);
                }
                else
                {
                    throw new std::exception("Too many value indices!");
                }
            }
            else if (value_size == 8)
            {
                decode_values<T, uint64_t>(data, offset, n_values, value_lookup);
                if (n_values < pow(2, 8))
                {
                    decode_indices<T, uint8_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 16))
                {
                    decode_indices<T, uint16_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 32))
                {
                    decode_indices<T, uint32_t>(data, offset, value_lookup, outBuffer);
                }
                else if (n_values < pow(2, 64))
                {
                    decode_indices<T, uint64_t>(data, offset, value_lookup, outBuffer);
                }
                else
                {
                    throw new std::exception("Too many value indices!");
                }
            }
            else
            {
                throw new std::exception("Value size too large, value cannot be longer than 8 bytes");
            }

            return 0;
        }
    }

    /// @brief Compress an array of numerical data using byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param data The data array to compress
    /// @param transposeBuffer An intermediate byte buffer to shuffle bytes into
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
        inner::transpose(data, transposeBuffer);
        auto outputBound = ZSTD_compressBound(transposeBuffer.size());
        outBuffer.resize(outputBound);
        auto used = ZSTD_compress(
            (void *)outBuffer.data(),
            outputBound,
            (void *)transposeBuffer.data(),
            transposeBuffer.size(),
            level);
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
    /// @param transposeBuffer An intermediate byte buffer to shuffle bytes into
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
        inner::reverse_tranpose(transposeBuffer, dataBuffer);
        return 0;
    }

    /// @brief Compress an array of numerical data using delta encoding, byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param data The data array to compress. *This array will be mutated with delta encoding*
    /// @param transposeBuffer An intermediate byte buffer to shuffle bytes into
    /// @param outBuffer A byte buffer to write ZSTD-compressed bytes to
    /// @param level The ZSTD compression level
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t delta_compress_buffer(
        std::vector<T> &data,
        buffer_t &transposeBuffer,
        buffer_t &outBuffer,
        int level = ZSTD_defaultCLevel())
    {
        inner::delta_encode(data);
        return compress_buffer(data, transposeBuffer, outBuffer);
    }

    /// @brief Decompress an array of numerical data using delta encoding, byte shuffling and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param buffer A byte buffer to containing ZSTD-compressed bytes
    /// @param transposeBuffer An intermediate byte buffer to shuffle bytes into
    /// @param dataBuffer The data array to decompress into
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t delta_decompress_buffer(
        const buffer_t &buffer,
        buffer_t &transposeBuffer,
        std::vector<T> &dataBuffer)
    {
        auto z = decompress_buffer(buffer, transposeBuffer, dataBuffer);
        inner::delta_decode(dataBuffer);
        return z;
    }

    /// @brief Compress an array of numerical data using dictionary encoding and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param data The data array to compress
    /// @param dictBuffer An intermediate byte buffer to hold the dictionary encoded bytes in
    /// @param outBuffer A byte buffer to write ZSTD-compressed bytes to
    /// @param level The ZSTD compression level
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t dict_compress_buffer(
        const std::vector<T> &data,
        buffer_t &dictBuffer,
        buffer_t &outBuffer,
        int level = ZSTD_defaultCLevel())
    {
        dictBuffer.clear();
        dict::dictionary_encode(data, dictBuffer);
        dictBuffer.shrink_to_fit();

        auto outputBound = ZSTD_compressBound(dictBuffer.size());
        outBuffer.resize(outputBound);
        auto used = ZSTD_compress(
            (void *)outBuffer.data(),
            outputBound,
            (void *)dictBuffer.data(),
            dictBuffer.size(),
            level);
        if (ZSTD_isError(used))
        {
            return used;
        }
        outBuffer.resize(used);
        return 0;
    }

    /// @brief Decompress an array of numerical data using dictionary encoding and ZSTD compression
    /// @tparam T The data type of the array to compress
    /// @param buffer A byte buffer to containing ZSTD-compressed bytes
    /// @param dictBuffer An intermediate byte buffer to hold the dictionary encoded bytes
    /// @param dataBuffer The data array to decompress into
    /// @return 0 if successful, some other value corresponding to a ZSTD error code otherwise
    template <typename T>
    size_t dict_decompress_buffer(
        const buffer_t &buffer,
        buffer_t &dictBuffer,
        std::vector<T> &dataBuffer)
    {
        dictBuffer.clear();
        auto outputBound = ZSTD_getFrameContentSize(buffer.data(), buffer.size());
        if (ZSTD_isError(outputBound))
        {
            return outputBound;
        }
        dictBuffer.resize(outputBound);
        auto used = ZSTD_decompress(
            (void *)dictBuffer.data(),
            outputBound,
            (void *)buffer.data(),
            buffer.size());
        dataBuffer.clear();
        return dict::dictionary_decode(
            dictBuffer,
            dataBuffer);
    }
}