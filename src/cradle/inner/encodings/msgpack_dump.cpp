#include <string>

#include <fmt/format.h>

#include <cradle/inner/encodings/msgpack_dump.h>

namespace cradle {

namespace {

std::string
make_indent(int indent)
{
    return std::string(indent, ' ');
}

} // namespace

// Note: incomplete implementation, does not cover all types
void
dump_msgpack_object(msgpack::object const& msgpack_obj, int indent)
{
    switch (msgpack_obj.type)
    {
        case msgpack::type::POSITIVE_INTEGER:
            fmt::print("POSITIVE_INTEGER {}\n", msgpack_obj.via.u64);
            break;
        case msgpack::type::ARRAY: {
            int size = static_cast<int>(msgpack_obj.via.array.size);
            fmt::print("ARRAY size {}\n", size);
            for (int i = 0; i < size; ++i)
            {
                fmt::print("{}[{}] ", make_indent(indent), i);
                dump_msgpack_object(msgpack_obj.via.array.ptr[i], indent + 4);
            }
        }
        break;
        case msgpack::type::STR: {
            int size = static_cast<int>(msgpack_obj.via.str.size);
            fmt::print("STR size {}", size);
            fmt::print(
                " \"{}\"\n", std::string(msgpack_obj.via.str.ptr, size));
        }
        break;
        default:
            fmt::print("UNKNOWN {}\n", (int) msgpack_obj.type);
            break;
    }
}

} // namespace cradle
