#ifndef CRADLE_INNER_ENCODINGS_MSGPACK_PACKER_H
#define CRADLE_INNER_ENCODINGS_MSGPACK_PACKER_H

#include <sstream>
#include <string>

#include <msgpack.hpp>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

// A string stream that class msgpack_packer packs into.
class msgpack_ostream
{
 public:
    void
    write(char const* data, std::streamsize count);

    blob
    get_blob() &&;

    std::string
    str() const;

 private:
    std::ostringstream ss_;
};

using msgpack_packer_base = msgpack::packer<msgpack_ostream>;

// A msgpack packer decorating the original one, and limited to writing to
// msgpack_ostream only.
// Note that the base class has no virtual destructor.
class msgpack_packer : public msgpack_packer_base
{
 public:
    msgpack_packer(msgpack_ostream& ostr, bool allow_blob_files);

    msgpack_ostream&
    ostr() const
    {
        return ostr_;
    }

    bool
    allow_blob_files() const
    {
        return allow_blob_files_;
    }

 private:
    msgpack_ostream& ostr_;
    bool allow_blob_files_;
};

} // namespace cradle

#endif
