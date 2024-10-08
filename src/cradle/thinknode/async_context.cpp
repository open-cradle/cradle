#include <chrono>
#include <thread>
#include <utility>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/requests/test_context.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/config.h>
#include <cradle/thinknode/async_context.h>
#include <cradle/thinknode/config.h>

namespace cradle {

std::string const the_domain_name{"thinknode"};

root_local_async_thinknode_context::root_local_async_thinknode_context(
    std::unique_ptr<local_tree_context_base> tree_ctx,
    service_config const& config)
    : root_local_async_context_base{*tree_ctx},
      test_params_context_mixin{config},
      owning_tree_ctx_{std::move(tree_ctx)}
{
}

root_local_async_thinknode_context::root_local_async_thinknode_context(
    std::unique_ptr<local_tree_context_base> tree_ctx,
    tasklet_tracker* tasklet)
    : root_local_async_context_base{*tree_ctx},
      owning_tree_ctx_{std::move(tree_ctx)}
{
    if (tasklet)
    {
        push_tasklet(*tasklet);
    }
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

void
root_local_async_thinknode_context::set_result(blob result)
{
    if (set_result_delay_ > 0)
    {
        auto& logger{get_tree_context().get_logger()};
        logger.warn("set_result() forced delay {}ms", set_result_delay_);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(set_result_delay_));
    }
    root_local_async_context_base::set_result(std::move(result));
}

void
root_local_async_thinknode_context::apply_fail_submit_async()
{
    if (fail_submit_async_)
    {
        auto& logger{get_tree_context().get_logger()};
        logger.warn("submit_async: forced failure");
        throw remote_error{"submit_async forced failure"};
    }
}

void
root_local_async_thinknode_context::apply_submit_async_delay()
{
    if (submit_async_delay_ > 0)
    {
        auto& logger{get_tree_context().get_logger()};
        logger.warn("submit_async() forced delay {}ms", submit_async_delay_);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(submit_async_delay_));
    }
}

void
root_local_async_thinknode_context::apply_resolve_async_delay()
{
    if (resolve_async_delay_ > 0)
    {
        auto& logger{get_tree_context().get_logger()};
        logger.warn(
            "resolve_async() forced startup delay {}ms", resolve_async_delay_);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(resolve_async_delay_));
    }
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

proxy_async_thinknode_tree_context::proxy_async_thinknode_tree_context(
    inner_resources& resources, std::string proxy_name)
    : proxy_async_tree_context_base{resources, std::move(proxy_name)}
{
}

root_proxy_async_thinknode_context::root_proxy_async_thinknode_context(
    std::unique_ptr<proxy_async_thinknode_tree_context> tree_ctx,
    thinknode_session session,
    tasklet_tracker* tasklet)
    : root_proxy_async_context_base{*tree_ctx},
      owning_tree_ctx_{std::move(tree_ctx)},
      session_{session},
      tasklet_{tasklet}
{
}

root_proxy_async_thinknode_context::~root_proxy_async_thinknode_context()
{
    finish_remote();
}

std::string const&
root_proxy_async_thinknode_context::domain_name() const
{
    return the_domain_name;
}

service_config
root_proxy_async_thinknode_context::make_config(bool need_record_lock) const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, the_domain_name},
        {remote_config_keys::NEED_RECORD_LOCK, need_record_lock},
        {thinknode_config_keys::API_URL, session_.api_url},
        {thinknode_config_keys::ACCESS_TOKEN, session_.access_token},
    };
    update_config_map_with_test_params(config_map);
    if (tasklet_)
    {
        config_map.insert(std::pair{
            remote_config_keys::TASKLET_ID,
            static_cast<std::size_t>(tasklet_->own_id())});
    }
    return service_config{config_map};
}

std::unique_ptr<proxy_async_context_base>
root_proxy_async_thinknode_context::make_sub_ctx(
    proxy_async_tree_context_base& tree_ctx, bool is_req)
{
    auto& my_tree_ctx{
        static_cast<proxy_async_thinknode_tree_context&>(tree_ctx)};
    return std::make_unique<non_root_proxy_async_thinknode_context>(
        my_tree_ctx, is_req);
}

non_root_proxy_async_thinknode_context::non_root_proxy_async_thinknode_context(
    proxy_async_thinknode_tree_context& tree_ctx, bool is_req)
    : non_root_proxy_async_context_base{tree_ctx, is_req}
{
}

std::string const&
non_root_proxy_async_thinknode_context::domain_name() const
{
    return the_domain_name;
}

service_config
non_root_proxy_async_thinknode_context::make_config(
    bool need_record_lock) const
{
    // Must be called only for a root context
    throw std::logic_error{
        "invalid non_root_proxy_async_thinknode_context::make_config() call"};
}

std::unique_ptr<proxy_async_context_base>
non_root_proxy_async_thinknode_context::make_sub_ctx(
    proxy_async_tree_context_base& tree_ctx, bool is_req)
{
    auto& my_tree_ctx{
        static_cast<proxy_async_thinknode_tree_context&>(tree_ctx)};
    return std::make_unique<non_root_proxy_async_thinknode_context>(
        my_tree_ctx, is_req);
}

async_thinknode_context::async_thinknode_context(
    inner_resources& resources,
    thinknode_session session,
    std::string proxy_name,
    std::optional<root_tasklet_spec> opt_tasklet_spec)
    : resources_{resources},
      proxy_name_{std::move(proxy_name)},
      session_{session},
      opt_tasklet_spec_{std::move(opt_tasklet_spec)},
      logger_{ensure_logger("async_thinknode")},
      preparation_future_{preparation_promise_.get_future()}
{
}

std::string const&
async_thinknode_context::domain_name() const
{
    return the_domain_name;
}

root_local_async_context_intf&
async_thinknode_context::prepare_for_local_resolution()
{
    logger_->info("prepare_for_local_resolution");
    if (!proxy_name_.empty())
    {
        // Should not be possible
        on_preparation_failed(std::logic_error{
            "invalid async_thinknode_context::prepare_for_local_resolution() "
            "call"});
    }
    local_root_ = std::make_shared<root_local_async_thinknode_context>(
        std::make_unique<local_tree_context_base>(resources_),
        create_optional_root_tasklet(
            resources_.the_tasklet_admin(), opt_tasklet_spec_));
    copy_test_params(*local_root_);
    register_local_async_ctx(local_root_);
    on_preparation_finished();
    return *local_root_;
}

remote_async_context_intf&
async_thinknode_context::prepare_for_remote_resolution()
{
    logger_->info("prepare_for_remote_resolution");
    if (proxy_name_.empty())
    {
        // Should not be possible
        on_preparation_failed(std::logic_error{
            "invalid async_thinknode_context::prepare_for_remote_resolution() "
            "call"});
    }
    remote_root_ = std::make_unique<root_proxy_async_thinknode_context>(
        std::make_unique<proxy_async_thinknode_tree_context>(
            resources_, proxy_name_),
        session_,
        create_optional_root_tasklet(
            resources_.the_tasklet_admin(), opt_tasklet_spec_));
    if (introspective_)
    {
        remote_root_->make_introspective();
    }
    copy_test_params(*remote_root_);
    on_preparation_finished();
    return *remote_root_;
}

void
async_thinknode_context::on_preparation_finished()
{
    // TODO prepare_..._resolution() calls may be from different threads
    // Even though the client may appear to be single-threaded, the
    // "co_await resolve_request()" may cause task switches.
    if (!prepared_)
    {
        preparation_promise_.set_value();
        prepared_ = true;
    }
}

void
async_thinknode_context::on_preparation_failed(
    std::exception const& exception_to_throw)
{
    try
    {
        throw exception_to_throw;
    }
    catch (std::exception const&)
    {
        preparation_promise_.set_exception(std::current_exception());
        throw;
    }
}

void
async_thinknode_context::wait_until_prepared()
{
    // TODO if the caller is a coroutine, a cppcoro mechanism may be better
    preparation_future_.wait();
}

root_local_async_context_base&
async_thinknode_context::get_local_root()
{
    wait_until_prepared();
    if (!local_root_)
    {
        // Should not be possible
        throw std::logic_error{
            "async_thinknode_context object has no local root"};
    }
    return *local_root_;
}

root_local_async_context_base const&
async_thinknode_context::get_local_root() const
{
    return const_cast<async_thinknode_context*>(this)->get_local_root();
}

root_proxy_async_thinknode_context&
async_thinknode_context::get_remote_root()
{
    wait_until_prepared();
    if (!remote_root_)
    {
        // Should not be possible
        throw std::logic_error{
            "async_thinknode_context object has no remote root"};
    }
    return *remote_root_;
}

root_proxy_async_thinknode_context const&
async_thinknode_context::get_remote_root() const
{
    return const_cast<async_thinknode_context*>(this)->get_remote_root();
}

async_context_intf&
async_thinknode_context::get_async_root()
{
    wait_until_prepared();
    async_context_intf* root{};
    if (proxy_name_.empty())
    {
        root = local_root_ ? &*local_root_ : nullptr;
    }
    else
    {
        root = remote_root_ ? &*remote_root_ : nullptr;
    }
    if (!root)
    {
        // Should not be possible
        throw std::logic_error{
            "async_thinknode_context object has no async root"};
    }
    return *root;
}

async_context_intf const&
async_thinknode_context::get_async_root() const
{
    return const_cast<async_thinknode_context*>(this)->get_async_root();
}

} // namespace cradle
