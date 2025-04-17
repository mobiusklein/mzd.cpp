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

    return 0;
}