#include <cradle/inner/encodings/lz4.h>

#include <boost/numeric/conversion/cast.hpp>

#include <lz4.h>

namespace cradle {

namespace lz4 {

std::size_t
max_compressed_size(std::size_t original_size)
{
    return boost::numeric_cast<std::size_t>(
        LZ4_compressBound(boost::numeric_cast<int>(original_size)));
}

std::size_t
compress(
    void* dst, std::size_t dst_size, void const* src, std::size_t src_size)
{
    int const compressed_size = LZ4_compress_default(
        reinterpret_cast<char const*>(src),
        reinterpret_cast<char*>(dst),
        boost::numeric_cast<int>(src_size),
        boost::numeric_cast<int>(dst_size));
    if (compressed_size <= 0)
    {
        CRADLE_THROW(lz4_error() << lz4_error_code_info(compressed_size));
    }
    return boost::numeric_cast<std::size_t>(compressed_size);
}

std::size_t
decompress(
    void* dst, std::size_t dst_size, void const* src, std::size_t src_size)
{
    int const decompressed_size = LZ4_decompress_safe(
        reinterpret_cast<char const*>(src),
        reinterpret_cast<char*>(dst),
        boost::numeric_cast<int>(src_size),
        boost::numeric_cast<int>(dst_size));
    if (decompressed_size < 0)
    {
        CRADLE_THROW(lz4_error() << lz4_error_code_info(decompressed_size));
    }
    return boost::numeric_cast<std::size_t>(decompressed_size);
}

} // namespace lz4

} // namespace cradle
