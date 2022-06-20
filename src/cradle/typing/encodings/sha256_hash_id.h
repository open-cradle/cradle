#ifndef CRADLE_TYPING_ENCODINGS_SHA256_HASH_ID_H
#define CRADLE_TYPING_ENCODINGS_SHA256_HASH_ID_H

#include <picosha2.h>

#include <spdlog/spdlog.h>

#include <cradle/inner/core/id.h>
#include <cradle/typing/encodings/native.h>

namespace cradle {

namespace detail {

template<class Value>
void
fold_into_sha256(picosha2::hash256_one_by_one& hasher, Value const& value)
{
    auto natively_encoded = write_natively_encoded_value(to_dynamic(value));
    hasher.process(natively_encoded.begin(), natively_encoded.end());
}

inline void
fold_into_sha256(
    picosha2::hash256_one_by_one& hasher, std::string const& value)
{
    hasher.process(value.begin(), value.end());
}

inline void
fold_into_sha256(picosha2::hash256_one_by_one& hasher, char const* value)
{
    hasher.process(value, value + strlen(value));
}

} // namespace detail

template<class... Args>
struct sha256_hashed_id : id_interface
{
    // Seems to be used in test code only
    sha256_hashed_id()
    {
    }

    sha256_hashed_id(std::tuple<Args...> args) : args_(std::move(args))
    {
    }

    bool
    equals(id_interface const& other) const override
    {
        sha256_hashed_id const& other_id
            = static_cast<sha256_hashed_id const&>(other);
        return args_ == other_id.args_;
    }

    bool
    less_than(id_interface const& other) const override
    {
        sha256_hashed_id const& other_id
            = static_cast<sha256_hashed_id const&>(other);
        return args_ < other_id.args_;
    }

    // Should prevent hash collisions in the disk cache
    void
    stream(std::ostream& o) const override
    {
        picosha2::hash256_one_by_one hasher;
        std::apply(
            [&hasher](auto... args) {
                (detail::fold_into_sha256(hasher, args), ...);
            },
            args_);
        hasher.finish();
        // Or use picosha2::get_hash_hex_string(hasher) because
        // ultimately we need a string.
        picosha2::byte_t hashed[32];
        hasher.get_hash_bytes(hashed, hashed + 32);
        picosha2::output_hex(hashed, hashed + 32, o);
        {
            std::ostringstream s;
            s << "sha256_hash_id::stream\n";
            std::apply(
                [&s](auto... args) {
                    ((s << "<- " << to_dynamic(args) << std::endl), ...);
                },
                args_);
            picosha2::output_hex(hashed, hashed + 32, s);
            spdlog::get("cradle")->debug(s.str());
        }
    }

    // aka hash_value()
    // Should prevent hash collisions in the memory cache
    size_t
    hash() const override
    {
        return std::apply(
            [](auto... args) { return combine_hashes(invoke_hash(args)...); },
            args_);
    }

 private:
    std::tuple<Args...> args_;
};

// TODO unused, consider removing
template<class... Args>
sha256_hashed_id<Args...>
make_sha256_hashed_id(Args... args)
{
    return sha256_hashed_id<Args...>(std::make_tuple(std::move(args)...));
}

template<class... Args>
captured_id
make_captured_sha256_hashed_id(Args... args)
{
    return captured_id{
        new sha256_hashed_id<Args...>(std::make_tuple(std::move(args)...))};
}

} // namespace cradle

#endif
