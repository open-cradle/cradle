#ifndef CRADLE_INNER_CORE_UNIQUE_HASH_H
#define CRADLE_INNER_CORE_UNIQUE_HASH_H

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <string>

#include <openssl/sha.h>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

struct id_interface;

// Get a string that is unique for the given ID (based on its hash).
// The primary purpose of these strings is to act as keys in the disk cache.
// That cache can contain items for all kinds of requests, including
// old-style Thinknode ones, and maybe even more.
//
// Preventing collisions is important.
// - Collisions between different types of requests will not appear assuming
//   their hash includes a uuid.
// - The number and types of a request's arguments are fixed, so also there
//   collisions cannot happen. (typing/core/unique_hash.h hashes dynamics,
//   taking types into account.)
//
// id should be one of:
// - A captured_id belonging to a fully-cached request (so carrying a uuid
//   at least in that request, possibly more in subrequests); or
// - A sha256_hashed_id calculated for an old-style Thinknode request
//
// An optimization for the first case could be to base the ultimate hash
// value only on the top request's uuid, but it doesn't seem worthwhile.
std::string
get_unique_string(id_interface const& id);

// Creates a cryptographic-strength hash value that should prevent collisions
// between different items written to the disk cache.
// Collisions between values that happen to have the same bitwise
// representation are prevented by prefixing them with their type.
class unique_hasher
{
 public:
    using byte_t = unsigned char;
    static constexpr size_t result_size = SHA256_DIGEST_LENGTH;

    struct result_t
    {
        byte_t bytes[result_size];
    };

    unique_hasher()
    {
        SHA256_Init(&ctx_);
    }

    void
    encode_bytes(void const* data, size_t len)
    {
        assert(!finished_);
        SHA256_Update(&ctx_, data, len);
    }

    void
    encode_bytes(byte_t const* begin, byte_t const* end)
    {
        encode_bytes(begin, end - begin);
    }

    void
    encode_bytes(std::byte const* begin, std::byte const* end)
    {
        encode_bytes(begin, end - begin);
    }

    void
    encode_bytes(char const* begin, char const* end)
    {
        encode_bytes(begin, end - begin);
    }

    // Update from a partial hash.
    void
    combine(result_t const& partial)
    {
        encode_bytes(partial.bytes, result_size);
    }

    void
    get_result(result_t& result)
    {
        finish();
        result = result_;
    }

    std::string
    get_string();

 private:
    void
    finish();

    SHA256_CTX ctx_;
    result_t result_;
    bool finished_{false};
};

template<typename T>
    requires std::integral<T> || std::floating_point<T>
void
update_unique_hash(unique_hasher& hasher, T val)
{
    char const* p = reinterpret_cast<char const*>(&val);
    hasher.encode_bytes(p, sizeof(val));
}

void
update_unique_hash(unique_hasher& hasher, std::string const& val);

void
update_unique_hash(unique_hasher& hasher, blob const& val);

} // namespace cradle

#endif
