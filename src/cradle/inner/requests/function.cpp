#include <stdexcept>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/function.h>

namespace cradle {

void
check_title_is_valid(std::string const& title)
{
    if (title.empty())
    {
        throw std::invalid_argument{"empty title"};
    }
}

using entry_t = cereal_functions_registry::entry_t;

static bool
operator==(entry_t const& lhs, entry_t const& rhs)
{
    return lhs.create == rhs.create && lhs.save == rhs.save
           && lhs.load == rhs.load;
}

static bool
operator!=(entry_t const& lhs, entry_t const& rhs)
{
    return !(lhs == rhs);
}

cereal_functions_registry&
cereal_functions_registry::instance()
{
    // The singleton is part of the main program, and any dynamically loaded
    // library will see this instance.
    static cereal_functions_registry the_instance;
    return the_instance;
}

// TODO add "DLL id" arg ane make it part of entry_t
void
cereal_functions_registry::add_entry(
    std::string const& uuid_str,
    create_t* create,
    save_t* save,
    load_t* load,
    unregister_t* unregister)
{
    // This function is called when a function_request_impl object is created,
    // so can be called multiple times for the same uuid. Especially for
    // a request like "normalization<blob,coro>", simultaneous calls are even
    // possible while registering the requests in
    // seri_catalog::register_resolver().
    // TODO still need mutex now that seri_catalog objects are better isolated?
    //
    // Normally, the create/save/load triple would be identical for all
    // add_entry() calls for a given uuid. This changes when request
    // implementations can be dynamically loaded. Assuming that the ODR holds
    // across DLLs, it should be OK to (de-)serialize requests implemented in
    // DLL X, calling the corresponding create/save/load functions implemented
    // in DLL Y. Until DLL Y is unloaded...
    //
    // For the normalization functions, it seems possible to avoid this problem
    // by registering the requests separately. That suggestion already appeared
    // in thinknode/seri_catalog.cpp.
    std::scoped_lock lock{mutex_};
    entry_t new_entry{create, save, load, unregister};
    auto it = entries_.find(uuid_str);
    // TODO a (DLL id, uuid) combination must occur only once; except for
    // "normalize_arg" uuids
    if (it != entries_.end() && it->second != new_entry)
    {
        auto logger{spdlog::get("cradle")};
        logger->warn(
            "cereal_functions_registry: conflicting entries for uuid {}",
            uuid_str);
    }
    entries_.insert_or_assign(std::move(uuid_str), std::move(new_entry));
}

// TODO add function to remove all entries for a given DLL
void
cereal_functions_registry::remove_entry_if_exists(std::string const& uuid_str)
{
    auto logger{spdlog::get("cradle")};
    logger->debug(
        "cereal_functions_registry: remove_entry_if_exists {}", uuid_str);
    std::scoped_lock lock{mutex_};
    auto it = find_checked(uuid_str);
    if (auto* unregister = it->second.unregister)
    {
        logger->debug("cereal_functions_registry: unregister {}", uuid_str);
        unregister(uuid_str);
    }
    entries_.erase(it);
}

// TODO maybe add "DLL id" arg as preferred one?
entry_t&
cereal_functions_registry::find_entry(request_uuid const& uuid)
{
    std::scoped_lock lock{mutex_};
    return find_checked(uuid.str())->second;
}

cereal_functions_registry::entries_t::iterator
cereal_functions_registry::find_checked(std::string const& uuid_str)
{
    auto it = entries_.find(uuid_str);
    if (it == entries_.end())
    {
        // This should be an internal error.
        throw unregistered_uuid_error{fmt::format(
            "cereal_functions_registry: no entry for {}", uuid_str)};
    }
    return it;
}

} // namespace cradle
