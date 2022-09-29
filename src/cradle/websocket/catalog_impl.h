#ifndef CRADLE_WEBSOCKET_CATALOG_IMPL_H
#define CRADLE_WEBSOCKET_CATALOG_IMPL_H

#include <memory>
#include <unordered_map>

#include <cereal/archives/json.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/typing/core/type_definitions.h>

namespace cradle {

/**
 * Resolves a JSON-encoded serialized Thinknode request to a dynamic
 *
 * Abstract base class
 */
class dynamic_resolver_intf
{
 public:
    virtual ~dynamic_resolver_intf() = default;

    virtual cppcoro::task<dynamic>
    resolve(thinknode_request_context& ctx, cereal::JSONInputArchive& iarchive)
        = 0;
};

/**
 * Resolves a JSON-encoded serialized Thinknode request to a dynamic
 *
 * Implementation for a request yielding a Value
 * - Fully cached
 * - Introspected
 * - The underlying function must be a coroutine
 * - A Value must be convertible to a dynamic
 */
template<typename Value>
class dynamic_resolver_impl : public dynamic_resolver_intf
{
 public:
    cppcoro::task<dynamic>
    resolve(thinknode_request_context& ctx, cereal::JSONInputArchive& iarchive)
        override
    {
        using Props = request_props<caching_level_type::full, true, true>;
        auto req{function_request_erased<Value, Props>(iarchive)};
        Value value = co_await resolve_request(ctx, req);
        dynamic result;
        to_dynamic(&result, value);
        co_return result;
    }
};

/**
 * Registry of resolvers that can deserialize and resolve a Thinknode request
 *
 * Singleton.
 * A request is characterized by its uuid (as a string).
 * A request is resolved to a dynamic.
 * The registry maps uuid's to type-erased dynamic_resolver_impl objects, so
 * contains references to dynamic_resolver_intf's.
 */
class dynamic_resolver_registry
{
 public:
    /**
     * Returns the singleton
     */
    static dynamic_resolver_registry&
    instance()
    {
        return instance_;
    }

    /**
     * Registers a dynamic resolver for a uuid
     */
    template<typename Value>
    void
    register_resolver(std::string const& uuid_str)
    {
        map_[uuid_str] = std::make_shared<dynamic_resolver_impl<Value>>();
    }

    /**
     * Resolves a JSON-encoded serialized request appearing in this registry
     *
     * The request is characterized by uuid_str.
     * Throws a uuid_error if the uuid does not appear in the registry.
     */
    cppcoro::task<dynamic>
    resolve(
        std::string const& uuid_str,
        thinknode_request_context& ctx,
        cereal::JSONInputArchive& iarchive)
    {
        std::shared_ptr<dynamic_resolver_intf> resolver;
        try
        {
            resolver = map_.at(uuid_str);
        }
        catch (std::out_of_range&)
        {
            std::ostringstream oss;
            oss << "no request registered with uuid " << uuid_str;
            throw uuid_error(oss.str());
        }
        co_return co_await resolver->resolve(ctx, iarchive);
    }

 private:
    static dynamic_resolver_registry instance_;
    std::unordered_map<std::string, std::shared_ptr<dynamic_resolver_intf>>
        map_;
};

} // namespace cradle

#endif
