#include <regex>
#include <sstream>

#include <fmt/format.h>

#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/meta_catalog.h>

namespace cradle {

meta_catalog&
meta_catalog::instance()
{
    static meta_catalog the_instance;
    return the_instance;
}

void
meta_catalog::add_catalog(seri_catalog& catalog)
{
    std::scoped_lock lock{mutex_};
    for (auto const& uuid_str : catalog.get_all_uuid_strs())
    {
        catalogs_map_.insert(std::make_pair(uuid_str, &catalog));
    }
}

void
meta_catalog::remove_catalog(seri_catalog& catalog)
{
    std::scoped_lock lock{mutex_};
    for (auto const& uuid_str : catalog.get_all_uuid_strs())
    {
        catalogs_map_.erase(uuid_str);
    }
}

cppcoro::task<serialized_result>
meta_catalog::resolve(local_context_intf& ctx, std::string seri_req)
{
    auto uuid_str{extract_uuid_str(seri_req)};
    auto resolver{find_resolver(uuid_str)};
    // TODO ensure resolver isn't unloaded while it's resolving
    return resolver->resolve(ctx, std::move(seri_req));
}

std::string
meta_catalog::extract_uuid_str(std::string const& seri_req)
{
    // The uuid appears in the JSON like
    //   "uuid": "rq_retrieve_immutable_object_func+gb6df901-dirty"
    // Retrieving the uuid from the JSON text is easier than parsing the JSON.
    std::regex re("\"uuid\": \"(.+?)\"");
    std::smatch match;
    if (!std::regex_search(seri_req, match, re))
    {
        throw uuid_error{fmt::format("no uuid found in JSON: {}", seri_req)};
    }
    return match[1].str();
}

std::shared_ptr<seri_resolver_intf>
meta_catalog::find_resolver(std::string const& uuid_str)
{
    std::scoped_lock lock{mutex_};
    seri_catalog* catalog{};
    try
    {
        catalog = catalogs_map_.at(uuid_str);
    }
    catch (std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "no resolver registered for uuid " << uuid_str;
        oss << ". Registered uuids are:";
        for (auto const& kv : catalogs_map_)
        {
            oss << " " << kv.first;
        }
        throw uuid_error{oss.str()};
    }
    return catalog->get_resolver(uuid_str);
}

} // namespace cradle
