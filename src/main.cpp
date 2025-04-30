#include<iostream>
#include<vector>
#include<string>
#include<algorithm>

#include "mzd.hpp"

int main() {
    std::cout << "Hello, world" << std::endl;

    std::vector<double> data = {1, 2, 3, 4, 5};
    buffer_t buffer;
    buffer_t transposeBuffer;

    std::vector<double> ref(data);

    std::cout << "Compressing" << std::endl;
    mzd::delta_compress_buffer(data, transposeBuffer, buffer);
    std::cout << "Buffer is now " << buffer.size() << " elements long" << std::endl;

    std::cout << "Decompressing" << std::endl;

    std::vector<double> revert;
    mzd::delta_decompress_buffer(buffer, transposeBuffer, revert);

    std::cout << "Decompressed size: " << revert.size() << std::endl;
    for(auto i = 0; i < revert.size(); i++) {
        std::cout << i << ": " << ref[i] << " " << revert[i] << std::endl;
    }

    buffer_t dict_buffer;
    std::cout << "Encoding dictionary" << std::endl;
    mzd::dict::dictionary_encode(ref, dict_buffer);
    std::vector<double> read_out;
    std::cout << "Decoding dictionary" << std::endl;
    try {
        mzd::dict::dictionary_decode(dict_buffer, read_out);
    } catch(std::exception &err) {
        std::cout << "Error: " << err.what() << std::endl;
        return 1;
    }
    for (auto i = 0; i < revert.size(); i++)
    {
        std::cout << i << ": " << ref[i] << " " << read_out[i] << std::endl;
    }

    return 0;
}