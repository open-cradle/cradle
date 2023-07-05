#include <regex>
#include <sstream>

#include <fmt/format.h>

#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_resolver.h>

namespace cradle {

seri_catalog&
seri_catalog::instance()
{
    static seri_catalog the_catalog;
    return the_catalog;
}

cppcoro::task<serialized_result>
seri_catalog::resolve(local_context_intf& ctx, std::string seri_req)
{
    auto uuid_str{find_uuid_str(seri_req)};
    auto resolver{find_resolver(uuid_str)};
    return resolver->resolve(ctx, std::move(seri_req));
}

std::string
seri_catalog::find_uuid_str(std::string const& seri_req)
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
seri_catalog::find_resolver(std::string const& uuid_str)
{
    std::shared_ptr<seri_resolver_intf> resolver;
    try
    {
        resolver = map_.at(uuid_str);
    }
    catch (std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "no request registered with uuid " << uuid_str;
        oss << ". Registered uuids are:";
        for (auto const& kv : map_)
        {
            oss << " " << kv.first;
        }
        throw uuid_error{oss.str()};
    }
    return resolver;
}

} // namespace cradle