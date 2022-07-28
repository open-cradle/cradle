#include <cassert>

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

void
unique_hasher::combine(result_t const& partial)
{
    assert(partial.initialized);
    auto begin = partial.bytes;
    auto end = begin + result_size;
    encode_bytes(begin, end);
}

void
unique_hasher::get_result(result_t& result)
{
    finish();
    auto begin = result.bytes;
    auto end = begin + result_size;
    impl_.get_hash_bytes(begin, end);
    result.initialized = true;
}

std::string
unique_hasher::get_string()
{
    finish();
    std::string res;
    picosha2::get_hash_hex_string(impl_, res);
    return res;
}

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
