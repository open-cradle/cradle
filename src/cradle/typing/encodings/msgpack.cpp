#include <cradle/typing/encodings/msgpack.h>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/utilities/text.h>
#include <cradle/typing/encodings/msgpack_internals.h>

namespace cradle {

object_handle_wrapper::object_handle_wrapper(uint8_t const* data, size_t size)
    : handle_{msgpack::unpack(reinterpret_cast<char const*>(data), size)}
{
}

static dynamic
read_msgpack_value(
    std::shared_ptr<data_owner> const& ownership,
    msgpack::object const& object)
{
    switch (object.type)
    {
        case msgpack::type::NIL:
        default:
            return nil;
        case msgpack::type::BOOLEAN:
            return object.via.boolean;
        case msgpack::type::POSITIVE_INTEGER:
            return boost::numeric_cast<integer>(object.via.u64);
        case msgpack::type::NEGATIVE_INTEGER:
            return boost::numeric_cast<integer>(object.via.i64);
        case msgpack::type::FLOAT:
            return boost::numeric_cast<double>(object.via.f64);
        case msgpack::type::STR: {
            string s;
            object.convert(s);
            return s;
        }
        case msgpack::type::BIN:
            return blob{
                ownership, as_bytes(object.via.bin.ptr), object.via.bin.size};
        case msgpack::type::ARRAY: {
            size_t size = object.via.array.size;
            dynamic_array array;
            array.reserve(size);
            for (size_t i = 0; i != size; ++i)
            {
                array.push_back(
                    read_msgpack_value(ownership, object.via.array.ptr[i]));
            }
            return array;
        }
        case msgpack::type::MAP: {
            dynamic_map map;
            for (size_t i = 0; i != object.via.map.size; ++i)
            {
                auto const& pair = object.via.map.ptr[i];
                map[read_msgpack_value(ownership, pair.key)]
                    = read_msgpack_value(ownership, pair.val);
            }
            return map;
        }
        case msgpack::type::EXT: {
            switch (object.via.ext.type())
            {
                case 1: // datetime
                {
                    int64_t t = 0;
                    auto const* data = object.via.ext.data();
                    switch (object.via.ext.size)
                    {
                        case 1:
                            t = *reinterpret_cast<int8_t const*>(data);
                            break;
                        case 2: {
                            uint16_t native_data
                                = boost::endian::big_to_native(
                                    *reinterpret_cast<uint16_t const*>(data));
                            t = *reinterpret_cast<int16_t const*>(
                                &native_data);
                            break;
                        }
                        case 4: {
                            uint32_t native_data
                                = boost::endian::big_to_native(
                                    *reinterpret_cast<uint32_t const*>(data));
                            t = *reinterpret_cast<int32_t const*>(
                                &native_data);
                            break;
                        }
                        case 8: {
                            uint64_t native_data
                                = boost::endian::big_to_native(
                                    *reinterpret_cast<uint64_t const*>(data));
                            t = *reinterpret_cast<int64_t const*>(
                                &native_data);
                            break;
                        }
                    }
                    return ptime(date(1970, 1, 1))
                           + boost::posix_time::milliseconds(t);
                }
                default:
                    CRADLE_THROW(
                        parsing_error()
                        << expected_format_info("MessagePack")
                        << parsing_error_info(
                               "unsupported MessagePack extension type"));
            }
            break;
        }
    }
}

dynamic
parse_msgpack_value(uint8_t const* data, size_t size)
{
    // msgpack::unpack returns a unique handle which contains the object and
    // also owns the data stored within the object. Copying the handle
    // transfers ownership of the data.
    // We want to be able to capture the blobs in the object without copying
    // all their data, so in order to do that, we create a shared_ptr to the
    // object handle and pass that in as the ownership_holder for the blobs to
    // use.
    auto wrapper{std::make_shared<object_handle_wrapper>(data, size)};
    return read_msgpack_value(wrapper, wrapper->handle().get());
}

dynamic
parse_msgpack_value(string const& msgpack)
{
    return parse_msgpack_value(
        reinterpret_cast<uint8_t const*>(msgpack.c_str()), msgpack.length());
}

// This is passed to the msgpack unpacker to tell it whether different types
// of objects should be copied out of the packed buffer or referenced
// directly.
static bool
msgpack_unpack_reference_type(msgpack::type::object_type type, size_t, void*)
{
    // Reference blobs directly, but copy anything else.
    return type == msgpack::type::BIN;
}

dynamic
parse_msgpack_value(
    std::shared_ptr<data_owner> const& owner, uint8_t const* data, size_t size)
{
    msgpack::object_handle handle = msgpack::unpack(
        reinterpret_cast<char const*>(data),
        size,
        msgpack_unpack_reference_type);
    return read_msgpack_value(owner, handle.get());
}

string
value_to_msgpack_string(dynamic const& v)
{
    std::stringstream stream;
    msgpack::packer<std::stringstream> packer(stream);
    write_msgpack_value(packer, v);
    return stream.str();
}

blob
value_to_msgpack_blob(dynamic const& v)
{
    auto wrapper{std::make_shared<sbuffer_wrapper>()};
    auto& sbuffer{wrapper->sbuffer()};
    msgpack::packer<msgpack::sbuffer> packer(sbuffer);
    write_msgpack_value(packer, v);
    std::byte const* data = as_bytes(sbuffer.data());
    size_t size = sbuffer.size();
    return blob{std::move(wrapper), data, size};
}

} // namespace cradle
