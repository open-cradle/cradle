#include <ostream>
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
dump_msgpack_object(
    std::ostream& os, msgpack::object const& msgpack_obj, int indent)
{
    switch (msgpack_obj.type)
    {
        case msgpack::type::POSITIVE_INTEGER:
            os << fmt::format("POSITIVE_INTEGER {}\n", msgpack_obj.via.u64);
            break;
        case msgpack::type::ARRAY: {
            int size = static_cast<int>(msgpack_obj.via.array.size);
            os << fmt::format("ARRAY size {}\n", size);
            for (int i = 0; i < size; ++i)
            {
                int child_indent = indent + 4;
                os << fmt::format("{}[{}] ", make_indent(child_indent), i);
                dump_msgpack_object(
                    os, msgpack_obj.via.array.ptr[i], child_indent);
            }
        }
        break;
        case msgpack::type::STR: {
            int size = static_cast<int>(msgpack_obj.via.str.size);
            os << fmt::format(
                "STR size {} \"{}\"\n",
                size,
                std::string(msgpack_obj.via.str.ptr, size));
        }
        break;
        default:
            os << fmt::format(
                "UNKNOWN type {}\n", static_cast<int>(msgpack_obj.type));
            break;
    }
}

} // namespace cradle
