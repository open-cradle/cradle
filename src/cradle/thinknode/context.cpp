#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/remote/config.h>
#include <cradle/thinknode/config.h>
#include <cradle/thinknode/context.h>

namespace cradle {

namespace {

std::string const the_domain_name{"thinknode"};

thinknode_session
make_session(service_config const& config)
{
    // Running on a remote server. Client must pass config including api_url
    // and access_token.
    return thinknode_session{
        config.get_mandatory_string(thinknode_config_keys::API_URL),
        config.get_mandatory_string(thinknode_config_keys::ACCESS_TOKEN)};
}

} // namespace

thinknode_request_context::thinknode_request_context(
    service_core& service, service_config const& config)
    : sync_context_base{service, nullptr, ""},
      service{service},
      session{make_session(config)}
{
}

thinknode_request_context::thinknode_request_context(
    service_core& service,
    thinknode_session session,
    tasklet_tracker* tasklet,
    std::string proxy_name)
    : sync_context_base{service, tasklet, std::move(proxy_name)},
      service{service},
      session{std::move(session)}
{
}

thinknode_request_context::~thinknode_request_context()
{
}

std::string const&
thinknode_request_context::domain_name() const
{
    return the_domain_name;
}

service_config
thinknode_request_context::make_config(bool need_record_lock) const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, domain_name()},
        {remote_config_keys::NEED_RECORD_LOCK, need_record_lock},
        {thinknode_config_keys::API_URL, session.api_url},
        {thinknode_config_keys::ACCESS_TOKEN, session.access_token},
    };
    if (!tasklets_.empty())
    {
        config_map.insert(std::pair{
            remote_config_keys::TASKLET_ID,
            static_cast<std::size_t>(tasklets_.back()->own_id())});
    }
    return service_config{config_map};
}

root_local_async_thinknode_context::root_local_async_thinknode_context(
    std::unique_ptr<local_tree_context_base> tree_ctx,
    service_config const& config)
    : root_local_async_context_base{*tree_ctx},
      session{make_session(config)},
      owning_tree_ctx_{std::move(tree_ctx)}
{
}

root_local_async_thinknode_context::root_local_async_thinknode_context(
    std::unique_ptr<local_tree_context_base> tree_ctx,
    thinknode_session session)
    : root_local_async_context_base{*tree_ctx},
      session{std::move(session)},
      owning_tree_ctx_{std::move(tree_ctx)}
{
}

std::string const&
root_local_async_thinknode_context::domain_name() const
{
    return the_domain_name;
}

std::unique_ptr<req_visitor_intf>
root_local_async_thinknode_context::make_ctx_tree_builder()
{
    return std::make_unique<local_async_thinknode_context_tree_builder>(*this);
}

std::string const&
non_root_local_async_thinknode_context::domain_name() const
{
    return the_domain_name;
}

local_async_thinknode_context_tree_builder::
    local_async_thinknode_context_tree_builder(local_async_context_base& ctx)
    : local_context_tree_builder_base{ctx}
{
}

std::unique_ptr<local_context_tree_builder_base>
local_async_thinknode_context_tree_builder::make_sub_builder(
    local_async_context_base& sub_ctx)
{
    return std::make_unique<local_async_thinknode_context_tree_builder>(
        sub_ctx);
}

std::shared_ptr<local_async_context_base>
local_async_thinknode_context_tree_builder::make_sub_ctx(
    local_tree_context_base& tree_ctx,
    std::size_t ix,
    bool is_req,
    std::unique_ptr<request_essentials> essentials)
{
    return std::make_shared<non_root_local_async_thinknode_context>(
        tree_ctx, &ctx_, is_req, std::move(essentials));
}

} // namespace cradle
