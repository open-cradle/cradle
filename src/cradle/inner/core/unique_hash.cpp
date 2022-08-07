#include <cassert>
#include <iomanip>
#include <sstream>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

std::string
get_unique_string(id_interface const& id)
{
    unique_hasher hasher;
    id.update_hash(hasher);
    return hasher.get_string();
}

std::string
unique_hasher::get_string()
{
    finish();
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < result_size; ++i)
    {
        ss << std::setw(2) << static_cast<unsigned int>(result_.bytes[i]);
    }
    return ss.str();
}

void
unique_hasher::finish()
{
    if (!finished_)
    {
        SHA256_Final(result_.bytes, &ctx_);
        finished_ = true;
    }
}

void
update_unique_hash(unique_hasher& hasher, std::string const& val)
{
    update_unique_hash(hasher, val.size());
    hasher.encode_bytes(val.data(), val.size());
}

void
update_unique_hash(unique_hasher& hasher, blob const& val)
{
    update_unique_hash(hasher, val.size());
    hasher.encode_bytes(val.data(), val.size());
}

} // namespace cradle
