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

void
cereal_functions_registry::add_entry(
    std::string const& uuid_str, create_t* create, save_t* save, load_t* load)
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
    //
    // TODO support unloading create/save/load
    auto logger{spdlog::get("cradle")};
    std::scoped_lock lock{mutex_};
    entry_t new_entry{create, save, load};
    auto it = entries_.find(uuid_str);
    if (it == entries_.end())
    {
        entries_[uuid_str] = new_entry;
    }
    else if (it->second != new_entry)
    {
        logger->warn("conflicting entries for uuid {}", uuid_str);
    }
}

entry_t&
cereal_functions_registry::find_entry(request_uuid const& uuid)
{
    std::scoped_lock lock{mutex_};
    std::string uuid_str{uuid.str()};
    auto it = entries_.find(uuid_str);
    if (it == entries_.end())
    {
        // This should be an internal error.
        throw unregistered_uuid_error{fmt::format(
            "cereal_functions_registry: no entry for {}", uuid_str)};
    }
    return it->second;
}

} // namespace cradle
