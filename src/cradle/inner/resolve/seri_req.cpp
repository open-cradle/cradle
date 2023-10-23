#include <regex>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/remote.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

namespace {

std::string
extract_uuid_str(std::string const& seri_req)
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

} // namespace

cppcoro::task<serialized_result>
resolve_serialized_remote(remote_context_intf& ctx, std::string seri_req)
{
    co_return resolve_remote(ctx, std::move(seri_req));
}

cppcoro::task<serialized_result>
resolve_serialized_local(local_context_intf& ctx, std::string seri_req)
{
    auto uuid_str{extract_uuid_str(seri_req)};
    auto& resources{ctx.get_resources()};
    auto resolver{resources.get_seri_registry()->find_resolver(uuid_str)};
    // TODO ensure resolver isn't unloaded while it's resolving
    return resolver->resolve(ctx, std::move(seri_req));
}

// Currently only called from websocket/server.cpp
cppcoro::task<serialized_result>
resolve_serialized_request(context_intf& ctx, std::string seri_req)
{
    if (auto* rem_ctx = cast_ctx_to_ptr<remote_context_intf>(ctx))
    {
        return resolve_serialized_remote(*rem_ctx, std::move(seri_req));
    }
    else
    {
        auto& loc_ctx = cast_ctx_to_ref<local_context_intf>(ctx);
        return resolve_serialized_local(loc_ctx, std::move(seri_req));
    }
}

} // namespace cradle
