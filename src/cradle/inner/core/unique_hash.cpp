#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

void
unique_hasher::finish()
{
    if (!finished_)
    {
        impl_.finish();
        finished_ = true;
    }
}

void
update_unique_hash(unique_hasher& hasher, std::string const& val)
{
    hasher.encode_type<std::string>();
    update_unique_hash(hasher, val.size());
    hasher.encode_bytes(val.data(), val.data() + val.size());
}

void
update_unique_hash(unique_hasher& hasher, blob const& val)
{
    hasher.encode_type<blob>();
    update_unique_hash(hasher, val.size());
    hasher.encode_bytes(val.data(), val.data() + val.size());
}

} // namespace cradle
