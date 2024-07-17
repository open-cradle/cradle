#ifndef CRADLE_INNER_CORE_UNIQUE_HASH_H
#define CRADLE_INNER_CORE_UNIQUE_HASH_H

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <openssl/sha.h>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

// Creates a cryptographic-strength hash value that should prevent collisions
// between different items written to the disk cache.
// The hash function is assumed to be so strong that collisions will not occur
// between different byte sequences fed to the hasher.
class unique_hasher
{
 public:
    using byte_t = unsigned char;
    static constexpr size_t result_size = SHA256_DIGEST_LENGTH;
    using result_t = std::array<unsigned char, result_size>;

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
        encode_bytes(partial.data(), result_size);
    }

    result_t
    get_result()
    {
        finish();
        return result_;
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
update_unique_hash(unique_hasher& hasher, char const* val);

void
update_unique_hash(unique_hasher& hasher, blob const& val);

void
update_unique_hash(unique_hasher& hasher, byte_vector const& val);

// TODO maybe try to split off all support for vector<T>
template<typename T>
void
update_unique_hash(unique_hasher& hasher, std::vector<T> const& val)
{
    update_unique_hash(hasher, val.size());
    for (auto const& x : val)
    {
        update_unique_hash(hasher, x);
    }
}

} // namespace cradle

#endif
