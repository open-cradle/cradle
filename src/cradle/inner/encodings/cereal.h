#ifndef CRADLE_INNER_ENCODINGS_CEREAL_H
#define CRADLE_INNER_ENCODINGS_CEREAL_H

// Serialization of CRADLE's inner types, using cereal

#include <utility>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/types.h>

namespace cradle {

inline void
save_blob_data(
    cereal::BinaryOutputArchive& archive, void const* data, std::size_t size)
{
    archive.saveBinary(data, size);
}

// A blob will typically contain binary data. cereal offers
// saveBinaryValue() and loadBinaryValue() that cause binary data to be
// stored as base64 in JSON and XML. JSON stores non-printable bytes
// as e.g. "\u0001" (500% overhead), so base64 (33% overhead) might be
// more efficient.
inline void
save_blob_data(
    cereal::JSONOutputArchive& archive, void const* data, std::size_t size)
{
    archive.saveBinaryValue(data, size, "blob");
}

template<typename Archive>
void
save(Archive& archive, blob const& x)
{
    if (auto owner = x.mapped_file_data_owner())
    {
        archive(cereal::make_nvp("as_file", true));
        archive(cereal::make_nvp("path", owner->mapped_file()));
        // TODO: Get rid of these casts.
        archive(cereal::make_nvp(
            "offset",
            x.data()
                - reinterpret_cast<std::byte const*>(
                    const_cast<data_owner*>(owner)->data())));
    }
    else
    {
        archive(cereal::make_nvp("as_file", false));
        archive(cereal::make_nvp("size", x.size()));
        save_blob_data(archive, x.data(), x.size());
    }
}

inline void
load_blob_data(
    cereal::BinaryInputArchive& archive, void* data, std::size_t size)
{
    archive.loadBinary(data, size);
}

inline void
load_blob_data(cereal::JSONInputArchive& archive, void* data, std::size_t size)
{
    archive.loadBinaryValue(data, size, "blob");
}

template<typename Archive>
void
load(Archive& archive, blob& x)
{
    bool as_file;
    archive(cereal::make_nvp("as_file", as_file));
    if (as_file)
    {
        std::string path;
        archive(cereal::make_nvp("path", path));
        std::size_t offset;
        archive(cereal::make_nvp("offset", offset));
        auto owner = std::make_shared<cradle::blob_file_reader>(
            file_path(std::move(path)));
        x.reset(owner, owner->bytes() + offset, owner->size());
    }
    else
    {
        // It's somewhat redundant to serialize the size as it's implied by
        // the base64 string, but the array passed to loadBinaryValue()
        // must have the appropriate size.
        std::size_t size;
        archive(cereal::make_nvp("size", size));
        auto owner = make_shared_buffer(size);
        load_blob_data(archive, owner->data(), size);
        x.reset(owner, owner->bytes(), size);
    }
}

} // namespace cradle

#endif
