#ifndef CRADLE_INNER_ENCODINGS_MSGPACK_DUMP_H
#define CRADLE_INNER_ENCODINGS_MSGPACK_DUMP_H

#include <iosfwd>

#include <msgpack.hpp>

namespace cradle {

void
dump_msgpack_object(
    std::ostream& os, msgpack::object const& msgpack_obj, int indent = 0);

} // namespace cradle

#endif
