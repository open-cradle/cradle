#ifndef CRADLE_INNER_REQUESTS_UUID_H
#define CRADLE_INNER_REQUESTS_UUID_H

#include <functional>
#include <stdexcept>
#include <string>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

/*
 * A request's uuid uniquely identifies its class and current implementation:
 * - It must change when the implementation's observable behaviour changes.
 * - It must be identical across runs of the same build.
 * - In case of a template request class, the uuid must identify a single
 *   instantiation. (Except for a template argument identifying the request's
 *   value type.)
 *
 * A uuid is used in:
 * - Calculating a disk-cache hash (when resolving a fully-cached request)
 * - Request serialization
 * - Cereal polymorphic map
 *
 * A uuid is not needed for resolving uncached or memory-cached requests.
 *
 * If a request type R0 is a template argument for another request R1, then
 * R0's value_type must be identical for all R1 instantiations.
 * (Can we enforce this?)
 *
 *   template<Request R0>
 *   requires(std::same_as<typename R0::value_type, common_type>)
 *   class R1 { ... };
 *
 * A value_request instantiation can have a "no meaning" uuid.
 *
 * For a function request, its uuid must cover the function *value* though;
 * so for two requests with different functions, their uuid must differ, even
 * though the functions' *types* might be identical.
 */

struct uuid_error : public std::logic_error
{
    uuid_error(std::string const& what) : std::logic_error(what)
    {
    }

    uuid_error(char const* what) : std::logic_error(what)
    {
    }
};

class request_uuid
{
 public:
    // Creates an "empty" uuid, which cannot be used for anything.
    // Also called in request deserialization (in that case, followed up by
    // a serialize() call).
    request_uuid() = default;

    // The base string should be unique within this application.
    // It will be combined with Git information to produce something unique
    // across application runs on different builds.
    explicit request_uuid(std::string const& base)
        : str_{combine(base, get_git_version())}
    {
        check_base(base);
    }

    // The caller promises that the base+version combination will be updated
    // when the request's implementation changes.
    explicit request_uuid(std::string const& base, std::string const& version)
        : str_{combine(base, version)}
    {
        check_base(base);
    }

    // Returns the full uuid (base + version)
    std::string const&
    str() const
    {
        return str_;
    }

    // Indicates whether a request with this uuid can be disk-cached
    bool
    disk_cacheable() const
    {
        return !str_.empty();
    }

    // Indicates whether a request with this uuid can be serialized
    bool
    serializable() const
    {
        return !str_.empty();
    }

    bool
    operator==(request_uuid const& other) const
    {
        return str_ == other.str_;
    }

    auto
    operator<=>(request_uuid const& other) const
    {
        return str_ <=> other.str_;
    }

    static std::string const&
    get_git_version();

 public:
    // cereal interface
    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(str_);
    }

 private:
    inline static std::string git_version_;
    std::string str_;

    void
    check_base(std::string const& base);

    std::string
    combine(std::string const& base, std::string const& version);
};

// For memory cache, unordered map
inline size_t
hash_value(request_uuid const& uuid)
{
    return invoke_hash(uuid.str());
}

// For unordered map
struct hash_request_uuid
{
    std::size_t
    operator()(request_uuid const& uuid) const
    {
        return hash_value(uuid);
    }
};

// For disk cache
inline void
update_unique_hash(unique_hasher& hasher, request_uuid const& uuid)
{
    update_unique_hash(hasher, uuid.str());
}

// Combines the uuid's of a main request and a subrequest
request_uuid
combined_uuid(request_uuid const& main_uuid, request_uuid const& sub_uuid);

} // namespace cradle

#endif
