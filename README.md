# mzd - byte shuffling and ZSTD compression for MS binary array data.

This header-only library is a proof-of-concept implementation in `src/mzd.hpp`. It requires that you also link the zstd library with your binary, as outlined in the example `./CMakeLists.txt`

 - `mzd::compress_buffer` and `mzd::decompress_buffer` are lossless compression
 - `mzd::delta_compress_buffer` and `mzd::delta_decompress_buffer` are lossy but can produce better compression ratios using delta encoding