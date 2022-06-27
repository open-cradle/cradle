#ifndef CRADLE_INNER_CORE_UNIQUE_HASH_H
#define CRADLE_INNER_CORE_UNIQUE_HASH_H

#include <cassert>
#include <concepts>
#include <cstring>
#include <string>
#include <typeinfo>

#include <picosha2.h>

namespace cradle {

// Creates a cryptographic-strength hash value that should prevent collisions
// between different items written to the disk cache.
// Collisions between values that happen to have the same bitwise
// representation are prevented by prefixing them with their type.
class unique_hasher
{
 public:
    unique_hasher()
    {
    }

    template<typename T>
    void
    encode_type()
    {
        assert(!finished_);
        // TODO
        // From https://en.cppreference.com/w/cpp/types/type_info/name
        // The returned string can be identical for several types and change
        // between invocations of the same program.
        const char* name = typeid(T).name();
        impl_.process(name, name + strlen(name));
    }

    template<typename Iter>
    void
    encode_value(Iter begin, Iter end)
    {
        assert(!finished_);
        impl_.process(begin, end);
    }

    // Indicates that the hash depends on one or more lambda functions, that
    // might change e.g. when the program is rebuilt.
    void
    using_lambda()
    {
        using_lambda_ = true;
    }

    std::string
    get_string()
    {
        finish();
        std::string res;
        picosha2::get_hash_hex_string(impl_, res);
        return res;
    }

 private:
    void
    finish();

    picosha2::hash256_one_by_one impl_;
    bool finished_{false};
    bool using_lambda_{false};
};

template<typename T>
requires std::integral<T> || std::floating_point<T>
void
update_unique_hash(unique_hasher& hasher, T val)
{
    hasher.encode_type<T>();
    char* p = reinterpret_cast<char*>(&val);
    hasher.encode_value(p, p + sizeof(val));
}

inline void
update_unique_hash(unique_hasher& hasher, std::string const& val)
{
    hasher.encode_type<std::string>();
    const char* p = val.c_str();
    hasher.encode_value(p, p + val.length());
}

struct id_interface;

std::string
create_unique_hash(id_interface const& id);

} // namespace cradle

#endif
