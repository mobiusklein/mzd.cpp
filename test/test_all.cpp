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
    std::vector<T> ref;
    std::vector<T> revert;
    buffer_t dict_buffer;
    std::vector<T> read_out;

    std::copy(data.begin(), data.end(), std::back_inserter(ref));
    assert(data.size() == ref.size());
    std::cout << data.size() << " " << ref.size() << std::endl;

    std::cout << "Dictionary Encoding =====================" << std::endl;
    mzd::dict::dictionary_encode<T>(ref, transposeBuffer, dict_buffer);
    try
    {
        mzd::dict::dictionary_decode(dict_buffer, read_out);
    }
    catch (std::exception &err)
    {
        std::cout << "Error: " << err.what() << std::endl;
        return 1;
    }

    std::printf("read_out size: %zu\n", read_out.size());
    for (auto i = 0; i < read_out.size(); i++)
    {
        std::cout << i << ": " << data[i] << " " << read_out[i] << std::endl;
        assert(data[i] == read_out[i]);
    }

    std::cout << "Dictionary Compression =====================" << std::endl;

    read_out.clear();
    revert.clear();
    dict_buffer.clear();

    mzd::dict_compress_buffer<T>(ref, dict_buffer, buffer);

    mzd::dict_decompress_buffer(buffer, dict_buffer, revert);

    std::printf("read_out size: %zu\n", read_out.size());

    for (auto i = 0; i < revert.size(); i++)
    {
        std::cout << i << ": " << data[i] << " " << revert[i] << std::endl;
        assert(data[i] == revert[i]);
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
    std::cout << "testing double ========================================" << std::endl;
    std::vector<double> data_double =
        {
            200.00018816645022000000,
            0.00000000000000000000,
            200.00043034083151000000,
            0.00000000000000000000,
            200.00067251579924000000,
            0.00000000000000000000,
            200.00091469135347000000,
            0.00000000000000000000,
            201.10647068550810000000,
            0.00000000000000000000,
            201.10671554643099000000,
            0.00000000000000000000,
            201.10696040795017000000,
            0.00000000000000000000,
            201.10720527006566000000,
            0.00000000000000000000,
            201.10745013277739000000,
            908.68475341796875000000,
            201.10769499608537000000,
            1266.26928710937500000000,
            201.10793985998967000000,
            1258.11450195312500000000,
            201.10818472449023000000,
            848.79339599609375000000,
            201.10842958958708000000,
            0.00000000000000000000,
            201.10867445528024000000,
            0.00000000000000000000,
            201.10891932156963000000,
            0.0000000000000000000,
            200,
            0,
            300,
            1,
            400,
            10,
            500,
            100,
            600,
            1000,
        };
    assert(test_codec(data_double) == 0);
    std::reverse(data_double.begin(), data_double.end());
    assert(test_codec(data_double) == 0);

    std::cout << "testing float ========================================" << std::endl;
    std::vector<float> data_float = {1, 2, 3, 4, 5};
    assert(test_codec(data_float) == 0);
    std::reverse(data_float.begin(), data_float.end());
    assert(test_codec(data_float) == 0);

    std::cout << "testing int ========================================" << std::endl;
    std::vector<int> data_int = {1, 2, 3, 4, 5};
    assert(test_codec(data_int) == 0);
    std::reverse(data_int.begin(), data_int.end());
    assert(test_codec(data_int) == 0);

    std::cout << "testing unsigned char ========================================" << std::endl;
    std::vector<uint8_t> data_ubyte = {1, 2, 3, 4, 5};
    assert(test_codec(data_ubyte) == 0);
    std::reverse(data_ubyte.begin(), data_ubyte.end());
    assert(test_codec(data_ubyte) == 0);

    std::cout << "testing empty input ========================================" << std::endl;
    data_double.clear();
    assert(test_codec(data_double) == 0);

    std::cout << "testing decoding empty byte buffer ========================================" << std::endl;
    test_decode_empty<double>();
    test_decode_empty<float>();
    test_decode_empty<int>();
    test_decode_empty<char>();

    return 0;
}