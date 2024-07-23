#include <utility>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_packer.h>

namespace cradle {

void
msgpack_ostream::write(char const* data, std::streamsize count)
{
    ss_.write(data, count);
}

blob
msgpack_ostream::get_blob() &&
{
    return make_blob(std::move(ss_).str());
}

std::string
msgpack_ostream::str() const
{
    return ss_.str();
}

msgpack_packer::msgpack_packer(msgpack_ostream& ostr, bool allow_blob_files)
    : msgpack::packer<msgpack_ostream>(ostr),
      ostr_{ostr},
      allow_blob_files_{allow_blob_files}
{
}

} // namespace cradle
