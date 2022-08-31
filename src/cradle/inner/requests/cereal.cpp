#include <iostream>
#include <sstream>
#include <stdexcept>

#include <cradle/inner/requests/cereal.h>

#define LOGGING 0

namespace cradle {

struct uuid_error : public std::logic_error
{
    uuid_error(std::string const& what) : std::logic_error(what)
    {
    }

    uuid_error(char const* what) : std::logic_error(what)
    {
    }
};

uuid_registry uuid_registry::instance_;

void
uuid_registry::add(std::string const& uuid, std::type_index key)
{
#if LOGGING
    std::cout << "uuid_registry.add(" << uuid << ")\n";
#endif
    add_to_map(uuid, key);
    add_to_inverse_map(uuid, key);
}

std::string const&
uuid_registry::find(std::type_index key)
{
#if LOGGING
    std::cout << "uuid_registry.find(" << cereal::util::demangle(key.name())
              << ")\n";
#endif
    auto it = map_.find(key);
    if (it == map_.end())
    {
        std::ostringstream oss;
        oss << "uuid_registry has no entry for "
            << cereal::util::demangle(key.name());
        throw uuid_error(oss.str());
    }
    std::string const& res = it->second;
#if LOGGING
    std::cout << "  found " << res << "\n";
#endif
    return res;
}

void
uuid_registry::add_to_map(std::string const& uuid, std::type_index key)
{
    auto it = map_.find(key);
    if (it != map_.end())
    {
#if LOGGING
        if (it->second == uuid)
        {
            std::cout << "  (was already defined)\n";
        }
        else
        {
            std::cout << "  WARNING previously defined as " << it->second
                      << "\n";
        }
#endif
    }
    else
    {
        map_[key] = uuid;
    }
}

// The inverse map is used to check that no uuid refers to multiple,
// different, types.
void
uuid_registry::add_to_inverse_map(std::string const& uuid, std::type_index key)
{
    auto it = inverse_map_.find(uuid);
    if (it != inverse_map_.end())
    {
        if (it->second == key)
        {
#if LOGGING
            std::cout << "  (was already defined)\n";
#endif
        }
        else
        {
            auto this_name = cereal::util::demangle(key.name());
            auto earlier_name = cereal::util::demangle(it->second.name());
            std::ostringstream oss;
            oss << "ERROR: uuid \"" << uuid << "\" refers to " << earlier_name
                << " and " << this_name;
            throw uuid_error(oss.str());
        }
    }
    else
    {
        inverse_map_.emplace(std::make_pair(uuid, key));
    }
}

} // namespace cradle
