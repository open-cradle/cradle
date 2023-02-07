#include <iostream>

#include <fmt/format.h>

#include <cradle/inner/requests/cereal.h>

#define LOGGING 0

namespace cradle {

uuid_registry uuid_registry::instance_;

void
uuid_registry::add(request_uuid const& uuid, std::type_index key)
{
#if LOGGING
    std::cout << "uuid_registry.add(" << uuid.str() << ")\n";
#endif
    add_to_map(uuid, key);
    add_to_inverse_map(uuid, key);
}

request_uuid const&
uuid_registry::find(std::type_index key)
{
#if LOGGING
    std::cout << "uuid_registry.find(" << cereal::util::demangle(key.name())
              << ")\n";
#endif
    auto it = map_.find(key);
    if (it == map_.end())
    {
        throw uuid_error(fmt::format(
            "uuid_registry has no entry for {}",
            cereal::util::demangle(key.name())));
    }
    request_uuid const& res = it->second;
#if LOGGING
    std::cout << "  found " << res.str() << "\n";
#endif
    return res;
}

void
uuid_registry::add_to_map(request_uuid const& uuid, std::type_index key)
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
            std::cout << "  WARNING previously defined as " << it->second.str()
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
uuid_registry::add_to_inverse_map(
    request_uuid const& uuid, std::type_index key)
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
            throw conflicting_types_uuid_error(fmt::format(
                "uuid \"{}\" refers to {} and {}",
                uuid.str(),
                cereal::util::demangle(it->second.name()),
                cereal::util::demangle(key.name())));
        }
    }
    else
    {
        inverse_map_.emplace(std::make_pair(uuid, key));
    }
}

} // namespace cradle
