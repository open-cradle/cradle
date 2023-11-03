#ifndef CRADLE_INNER_RESOLVE_SERI_REGISTRY_H
#define CRADLE_INNER_RESOLVE_SERI_REGISTRY_H

#include <any>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include <cradle/inner/requests/types.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/seri_resolver.h>

namespace cradle {

class unregistered_uuid_error : public uuid_error
{
 public:
    using uuid_error::uuid_error;
};

/*
 * Registry helping the deserialization process create the correct
 * function_request_impl objects.
 *
 * The registry consists of entries associated with some uuid. An entry
 * contains:
 * - A `cat_id` number identifying a catalog of request types.
 * - A `create` function that creates the correct function_request_impl
 *   instantiation for the uuid.
 * - A `function` value, to be copied to function_request_impl::function_.
 *   A single function_request_impl instantiation can be related to multiple
 *   functions. All functions have the same signature, but are identified by
 *   different uuid's.
 *
 * When the catalog belongs to a DLL, the `create` and `function`
 * implementations are code inside that DLL, so when the DLL is unloaded, the
 * pointers become dangling and the entry has to be removed.
 *
 * This registry forms the basis for an ad-hoc alternative to the cereal
 * polymorphic types implementation, which is not usable here:
 * 1. A polymorphic type needs to be registered with CEREAL_REGISTER_TYPE or
 *    CEREAL_REGISTER_TYPE_WITH_NAME. These put constructs in namespaces, so
 *    cannot be used within a class or function.
 *    However, with templated classes like function_request_impl, we cannot use
 *    these constructs outside a class either, as the type of an instance is
 *    known only _within_ the class.
 * 2. Cereal needs to translate the type of a derived class to the name under
 *    which that polymorphic type will be serialized.
 *    - We cannot use type_index::name() because that is not portable, and
 *      because creating a serialized request (e.g. from a WebSocket client)
 *      would be practically impossible.
 *    - We cannot use the uuid because that is unique for a
 *      function_request_impl+function combination, not for just
 *      function_request_impl.
 * 3. Functions (function values) cannot be serialized.
 * Instead, we need a mechanism where the uuid in the serialization identifies
 * both the function_request_impl _class_ (template instantiation), and the
 * function _value_ in that class.
 *
 * All functions in this class's API are thread-safe.
 */
class seri_registry
{
 public:
    using create_t = std::shared_ptr<void>(request_uuid const& uuid);
    template<typename Function>
    using function_t = std::shared_ptr<Function>;
    using resolver_t = std::shared_ptr<seri_resolver_intf>;

    seri_registry();

    template<typename Function>
    void
    add(catalog_id cat_id,
        std::string const& uuid_str,
        resolver_t resolver,
        create_t* create,
        function_t<Function> function)
    {
        add(cat_id, uuid_str, std::move(resolver), create, std::any{function});
    }

    /*
     * To be called when a DLL is unloaded.
     *
     * Removes all entries containing pointers into the DLL's code.
     * Should something go wrong with the unregister (e.g. an exception that is
     * not handled properly), the registry is left with stale entries; their
     * stale cat_id's ensure they will not be accessed.
     */
    void
    unregister_catalog(catalog_id cat_id);

    // Intf should be a function_request_intf instantiation.
    template<typename Intf>
    std::shared_ptr<Intf>
    create(request_uuid const& uuid)
    {
        entry_t& entry{find_entry(uuid.str(), false)};
        return std::static_pointer_cast<Intf>(entry.create(uuid));
    }

    template<typename Function>
    function_t<Function>
    find_function(std::string const& uuid_str)
    {
        return std::any_cast<function_t<Function>>(
            find_entry(uuid_str, false).function);
    }

    resolver_t
    find_resolver(std::string const& uuid_str)
    {
        return find_entry(uuid_str, true).resolver;
    }

    std::size_t
    size() const;

    void
    log_all_entries(std::string const& when);

 private:
    struct entry_t
    {
        catalog_id cat_id;
        resolver_t resolver;
        create_t* create;
        std::any function; // wrapping function_t
    };

    // inner_list_t contains the entries for some uuid.
    // - List length should normally be 1.
    // - Empty lists are not possible (when a list's last entry is removed, the
    //   list itself is removed as well).
    using inner_list_t = std::list<entry_t>;
    // outer_map_t maps uuid strings to inner_list_t lists.
    using outer_map_t = std::unordered_map<std::string, inner_list_t>;

    std::shared_ptr<spdlog::logger> logger_;
    std::mutex mutex_;
    outer_map_t entries_;

    void
    add(catalog_id cat_id,
        std::string const& uuid_str,
        resolver_t resolver,
        create_t* create,
        std::any function);

    entry_t&
    find_entry(std::string const& uuid_str, bool verbose);

    std::string
    make_uuid_error_message(std::string const& uuid_str, bool verbose) const;

    bool
    detect_duplicate(
        inner_list_t const& inner_list,
        catalog_id cat_id,
        std::string const& uuid_str);
};

} // namespace cradle

#endif
