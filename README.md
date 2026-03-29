# mzd - byte shuffling and ZSTD compression for MS binary array data.

This header-only library is a proof-of-concept implementation in `src/mzd.hpp`. It requires that you also link the `zstd` library with your binary, as outlined in the example `./CMakeLists.txt`. All of the array compression methods in this library enforce storing data at-rest in little endian layout.

 - `mzd::compress_buffer` and `mzd::decompress_buffer` is a thin wrapper around `zstd`'s direct buffer compression codec.
 - `mzd::byteshuffle_compress_buffer` and `mzd::byteshuffle_decompress_buffer` perform better on sorted data.
 - `mzd::dict_compress_buffer` and `mzd::dict_decompress_buffer` are intended for arrays with repeated values (e.g. ion mobility, m/z profiles with ion mobility, charge state) and is an extension of the previous codec.

Some of the code for handling endianness and testing was adapted from [ProteoWizard](https://github.com/ProteoWizard/pwiz) during its integration there.