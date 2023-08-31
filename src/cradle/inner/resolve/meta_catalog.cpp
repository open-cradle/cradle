#include <regex>
#include <sstream>

#include <fmt/format.h>

#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/meta_catalog.h>
#include <cradle/inner/resolve/meta_catalog_impl.h>

namespace cradle {

meta_catalog&
meta_catalog::instance()
{
    static meta_catalog the_instance;
    return the_instance;
}

meta_catalog::meta_catalog() : impl_{std::make_unique<meta_catalog_impl>()}
{
}

void
meta_catalog::add_static_catalog(seri_catalog& catalog)
{
    impl_->add_static_catalog(catalog);
}

void
meta_catalog::add_dll_catalog(
    seri_catalog& catalog,
    std::string dll_path,
    std::unique_ptr<boost::dll::shared_library> lib)
{
    impl_->add_dll_catalog(catalog, std::move(dll_path), std::move(lib));
}

cppcoro::task<serialized_result>
meta_catalog::resolve(local_context_intf& ctx, std::string seri_req)
{
    return impl_->resolve(ctx, std::move(seri_req));
}

void
meta_catalog_impl::add_static_catalog(seri_catalog& catalog)
{
    add_controller(std::make_unique<static_catalog_controller>(catalog));
}

void
meta_catalog_impl::add_dll_catalog(
    seri_catalog& catalog,
    std::string dll_path,
    std::unique_ptr<boost::dll::shared_library> lib)
{
    add_controller(std::make_unique<dll_catalog_controller>(
        catalog, std::move(dll_path), std::move(lib)));
}

void
meta_catalog_impl::add_controller(
    std::unique_ptr<seri_catalog_controller> controller)
{
    auto const& catalog{controller->get_catalog()};
    for (auto const& uuid_str : catalog.get_all_uuid_strs())
    {
        controllers_by_uuid_.insert(std::make_pair(uuid_str, &*controller));
    }
    controllers_.push_back(std::move(controller));
}

cppcoro::task<serialized_result>
meta_catalog_impl::resolve(local_context_intf& ctx, std::string seri_req)
{
    auto uuid_str{find_uuid_str(seri_req)};
    auto resolver{find_resolver(uuid_str)};
    return resolver->resolve(ctx, std::move(seri_req));
}

std::string
meta_catalog_impl::find_uuid_str(std::string const& seri_req)
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
meta_catalog_impl::find_resolver(std::string const& uuid_str)
{
    seri_catalog_controller* controller{};
    try
    {
        controller = controllers_by_uuid_.at(uuid_str);
    }
    catch (std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "no resolver registered for uuid " << uuid_str;
        oss << ". Registered uuids are:";
        for (auto const& kv : controllers_by_uuid_)
        {
            oss << " " << kv.first;
        }
        throw uuid_error{oss.str()};
    }
    return controller->get_catalog().get_resolver(uuid_str);
}

} // namespace cradle
