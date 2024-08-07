#ifndef CRADLE_INNER_ENCODINGS_MSGPACK_ADAPTORS_MAIN_H
#define CRADLE_INNER_ENCODINGS_MSGPACK_ADAPTORS_MAIN_H

// Serialize CRADLE types from/to MessagePack
// For official msgpack versions

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <msgpack.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_packer.h>
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
            if (o.type == msgpack::type::STR)
            {
                // Should be the name of a blob file
                std::string name{o.via.str.ptr, o.via.str.size};
                auto owner = std::make_shared<cradle::blob_file_reader>(
                    cradle::file_path(std::move(name)));
                v.reset(owner, owner->bytes(), owner->size());
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
            static_assert(std::same_as<Stream, cradle::msgpack_ostream>);
            // Assuming that msgpack_packer is used everywhere, no
            // msgpack::pack() calls
            auto& packer = static_cast<cradle::msgpack_packer&>(o);
            if (auto owner = v.mapped_file_data_owner();
                owner && packer.allow_blob_files())
            {
                std::string name{owner->mapped_file()};
                uint32_t size{static_cast<uint32_t>(name.size())};
                o.pack_str(size);
                o.pack_str_body(name.c_str(), size);
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
        // Only called from tests/inner/encodings/msgpack_adaptors_main.cpp
        // in tests explicitly demanding a zone.
        void
        operator()(msgpack::object::with_zone& o, cradle::blob const& v) const
        {
            if (auto owner = v.mapped_file_data_owner())
            {
                std::string name{owner->mapped_file()};
                uint32_t size = static_cast<uint32_t>(name.size());
                o.type = msgpack::type::STR;
                char* ptr = static_cast<char*>(
                    o.zone.allocate_align(size, MSGPACK_ZONE_ALIGNOF(char)));
                o.via.str.ptr = ptr;
                o.via.str.size = size;
                std::memcpy(ptr, name.c_str(), size);
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
