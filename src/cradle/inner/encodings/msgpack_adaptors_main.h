#ifndef CRADLE_INNER_ENCODINGS_MSGPACK_ADAPTORS_MAIN_H
#define CRADLE_INNER_ENCODINGS_MSGPACK_ADAPTORS_MAIN_H

// Serialize CRADLE types from/to MessagePack
// For official msgpack versions

#include <cstring>
#include <stdexcept>
#include <utility>

#include <msgpack.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/types.h>

namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
    namespace adaptor {

    // TODO avoid data copies
    template<>
    struct convert<cradle::blob>
    {
        msgpack::object const&
        operator()(msgpack::object const& o, cradle::blob& v) const
        {
            if (o.type == msgpack::type::ARRAY)
            {
                // Should be the name of a blob file and an offset
                if (o.via.array.size != 2)
                    throw msgpack::type_error();
                msgpack::object const& file = o.via.array.ptr[0];
                if (file.type != msgpack::type::STR)
                    throw msgpack::type_error();
                msgpack::object const& offset = o.via.array.ptr[1];
                if (offset.type != msgpack::type::POSITIVE_INTEGER)
                    throw msgpack::type_error();
                std::string name{file.via.str.ptr, file.via.str.size};
                auto owner = std::make_shared<cradle::blob_file_reader>(
                    cradle::file_path(std::move(name)));
                v.reset(owner, owner->bytes() + offset.via.u64, owner->size());
                return o;
            }
            if (o.type != msgpack::type::BIN)
            {
                throw msgpack::type_error();
            }
            std::size_t size = o.via.bin.size;
            cradle::byte_vector bv(size);
            if (size != 0)
            {
                std::memcpy(&bv.front(), o.via.bin.ptr, size);
            }
            v = cradle::make_blob(bv);
            return o;
        }
    };

    template<>
    struct pack<cradle::blob>
    {
        template<typename Stream>
        msgpack::packer<Stream>&
        operator()(msgpack::packer<Stream>& o, cradle::blob const& v) const
        {
            if (auto owner = v.mapped_file_data_owner())
            {
                std::string name{owner->mapped_file()};
                uint32_t size{static_cast<uint32_t>(name.size())};
                o.pack_array(2);
                o.pack_str(size);
                o.pack_str_body(name.c_str(), size);
                uint64_t offset
                    = v.data()
                      - reinterpret_cast<std::byte const*>(
                          const_cast<cradle::data_owner*>(owner)->data());
                o.pack_uint64(offset);
                return o;
            }
            if (v.size() >= 0x1'00'00'00'00)
            {
                throw std::length_error("blob size >= 4GB");
            }
            uint32_t size = static_cast<uint32_t>(v.size());
            o.pack_bin(size);
            o.pack_bin_body(reinterpret_cast<char const*>(v.data()), size);
            return o;
        }
    };

    template<>
    struct object_with_zone<cradle::blob>
    {
        void
        operator()(msgpack::object::with_zone& o, cradle::blob const& v) const
        {
            if (auto owner = v.mapped_file_data_owner())
            {
                std::string name{owner->mapped_file()};
                uint64_t offset
                    = v.data()
                      - reinterpret_cast<std::byte const*>(
                          const_cast<cradle::data_owner*>(owner)->data());
                o.type = type::ARRAY;
                o.via.array.size = 2;
                o.via.array.ptr
                    = static_cast<msgpack::object*>(o.zone.allocate_align(
                        sizeof(msgpack::object) * o.via.array.size,
                        MSGPACK_ZONE_ALIGNOF(msgpack::object)));
                o.via.array.ptr[0] = msgpack::object(name, o.zone);
                o.via.array.ptr[1] = msgpack::object(offset, o.zone);
                return;
            }
            if (v.size() >= 0x1'00'00'00'00)
            {
                throw std::length_error("blob size >= 4GB");
            }
            uint32_t size = static_cast<uint32_t>(v.size());
            o.type = msgpack::type::BIN;
            o.via.bin.size = size;
            if (size != 0)
            {
                char* ptr = static_cast<char*>(
                    o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
                o.via.bin.ptr = ptr;
                std::memcpy(ptr, v.data(), size);
            }
        }
    };

    } // namespace adaptor
} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack

#endif
