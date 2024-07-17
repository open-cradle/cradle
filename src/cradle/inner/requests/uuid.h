#ifndef CRADLE_INNER_REQUESTS_UUID_H
#define CRADLE_INNER_REQUESTS_UUID_H

#include <functional>
#include <stdexcept>
#include <string>

#include <cereal/cereal.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

class msgpack_packer;

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
 * Without a uuid, neither of these are possible.
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
 * A value_request instantiation need not have a uuid.
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
    // The base string should be universally unique.
    explicit request_uuid(std::string base);

    struct complete_tag
    {
    };

    // Intended for when a complete uuid string is transmitted over RPC or
    // other channel, and the receiving side has to create a request_uuid
    // object from it.
    // Not intended for user code.
    request_uuid(std::string complete, complete_tag);

    request_uuid
    clone() const;

    // Causes the base uuid to be extended with something depending on the
    // caching level.
    // To be called when the corresponding request is a function class having
    // the caching level as template parameter.
    request_uuid&
    set_level(caching_level_type level);

    // Lets this uuid refer to a request that is a "flattened clone" of an
    // original one.
    request_uuid&
    set_flattened();

    // Returns the full uuid (base + any extensions)
    std::string const&
    str() const
    {
        finalize();
        return str_;
    }

    bool
    operator==(request_uuid const& other) const
    {
        return str() == other.str();
    }

    auto
    operator<=>(request_uuid const& other) const
    {
        return str() <=> other.str();
    }

    // Serialize using cereal.
    // This results in JSON
    //    "uuid": "...uuid text...",
    // A more usual implementation (e.g. putting serialize() in the class)
    // would give
    //    "uuid": { "value0": "...uuid text..." },
    template<typename Archive>
    void
    save_with_name(Archive& archive, std::string const& name) const
    {
        archive(cereal::make_nvp(name, str()));
    }

    // Deserialize using cereal.
    template<typename Archive>
    static request_uuid
    load_with_name(Archive& archive, std::string const& name)
    {
        request_uuid uuid;
        archive(cereal::make_nvp(name, uuid.str_));
        return uuid;
    }

    void
    save(msgpack_packer& packer) const;

    // MsgpackObj should be msgpack::object.
    // The template is a workaround for conflicts between the two msgpack
    // implementations (the main one, and the one inside rpclib), that lead to
    // build errors when trying to #include <msgpack.hpp> here.
    template<typename MsgpackObj>
    static request_uuid
    load(MsgpackObj const& msgpack_obj);

 private:
    // Used in load_with_name() only
    request_uuid() = default;

    // str_ is the full string if finalized_, base until then
    mutable std::string str_;
    mutable bool finalized_{false};

    // Modifiers; not used (anymore) when finalized_
    bool include_level_{false};
    caching_level_type level_{};
    bool flattened_{false};

    void
    check_not_finalized() const;

    void
    finalize() const
    {
        if (!finalized_)
        {
            do_finalize();
        }
    }

    void
    do_finalize() const;
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

} // namespace cradle

#endif
