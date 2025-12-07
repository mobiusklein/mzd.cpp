#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#include "../src/mzd.hpp"

template <typename T>
int test_codec(std::vector<T> &data)
{
    buffer_t buffer;
    buffer_t transposeBuffer;

    std::vector<T> ref(data);
    mzd::delta_compress_buffer(data, transposeBuffer, buffer);

    std::vector<T> revert;
    mzd::delta_decompress_buffer(buffer, transposeBuffer, revert);

    for (auto i = 0; i < revert.size(); i++)
    {
        // std::cout << i << ": " << ref[i] << " " << revert[i] << std::endl;
        assert(ref[i] == revert[i]);
    }

    transposeBuffer.clear();
    buffer_t dict_buffer;
    mzd::dict::dictionary_encode(ref, transposeBuffer, dict_buffer);
    std::vector<T> read_out;
    try
    {
        mzd::dict::dictionary_decode(dict_buffer, read_out);
    }
    catch (std::exception &err)
    {
        std::cout << "Error: " << err.what() << std::endl;
        return 1;
    }
    for (auto i = 0; i < revert.size(); i++)
    {
        // std::cout << i << ": " << ref[i] << " " << read_out[i] << std::endl;
        assert(ref[i] == read_out[i]);
    }

    revert.clear();
    mzd::dict_compress_buffer(ref, dict_buffer, buffer);
    mzd::dict_decompress_buffer(buffer, dict_buffer, revert);

    for (auto i = 0; i < revert.size(); i++)
    {
        // std::cout << i << ": " << ref[i] << " " << read_out[i] << std::endl;
        assert(ref[i] == read_out[i]);
    }
    return 0;
}

template <typename T>
int test_decode_empty() {
    std::vector<uint8_t> empty;
    std::vector<uint8_t> transposeBuffer;
    std::vector<T> outBuffer;

    mzd::byteshuffle_decompress_buffer(empty, transposeBuffer, outBuffer);
    assert(outBuffer.size() == 0);

    mzd::dict_decompress_buffer(empty, transposeBuffer, outBuffer);
    assert(outBuffer.size() == 0);
    return 0;
}

int main()
{
    std::cout << "testing double" << std::endl;
    std::vector<double> data_double = {1, 2, 3, 4, 5};
    assert(test_codec(data_double) == 0);
    std::reverse(data_double.begin(), data_double.end());
    assert(test_codec(data_double) == 0);

    std::cout << "testing float" << std::endl;
    std::vector<float> data_float = {1, 2, 3, 4, 5};
    assert(test_codec(data_float) == 0);
    std::reverse(data_float.begin(), data_float.end());
    assert(test_codec(data_float) == 0);

    std::cout << "testing int" << std::endl;
    std::vector<int> data_int = {1, 2, 3, 4, 5};
    assert(test_codec(data_int) == 0);
    std::reverse(data_int.begin(), data_int.end());
    assert(test_codec(data_int) == 0);

    std::cout << "testing unsigned char" << std::endl;
    std::vector<uint8_t> data_ubyte = {1, 2, 3, 4, 5};
    assert(test_codec(data_ubyte) == 0);
    std::reverse(data_ubyte.begin(), data_ubyte.end());
    assert(test_codec(data_ubyte) == 0);

    std::cout << "testing empty input" << std::endl;
    data_double.clear();
    assert(test_codec(data_double) == 0);

    std::cout << "testing decoding empty byte buffer" << std::endl;
    test_decode_empty<double>();
    test_decode_empty<float>();
    test_decode_empty<int>();
    test_decode_empty<char>();

    return 0;
}