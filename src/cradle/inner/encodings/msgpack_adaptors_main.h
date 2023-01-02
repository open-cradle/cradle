#ifndef CRADLE_INNER_ENCODINGS_MSGPACK_ADAPTORS_MAIN_H
#define CRADLE_INNER_ENCODINGS_MSGPACK_ADAPTORS_MAIN_H

// Serialize CRADLE types from/to MessagePack
// For official msgpack versions

#include <cstring>
#include <stdexcept>

#include <msgpack.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>

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
            if (o.type != msgpack::type::BIN)
            {
                throw msgpack::type_error();
            }
            std::size_t size = o.via.bin.size;
            cradle::byte_vector bv(size);
            std::memcpy(&bv.front(), o.via.bin.ptr, size);
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
