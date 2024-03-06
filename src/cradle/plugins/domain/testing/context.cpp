#include <chrono>
#include <thread>
#include <utility>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/config.h>
#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

std::string const the_domain_name{"testing"};

testing_request_context::testing_request_context(
    inner_resources& resources,
    std::string proxy_name,
    std::optional<root_tasklet_spec> opt_tasklet_spec)
    : sync_context_base{
        resources,
        create_optional_root_tasklet(
            resources.the_tasklet_admin(), std::move(opt_tasklet_spec)),
        std::move(proxy_name)}
{
}

std::string const&
testing_request_context::domain_name() const
{
    return the_domain_name;
}

service_config
testing_request_context::make_config(bool need_record_lock) const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, the_domain_name},
        {remote_config_keys::NEED_RECORD_LOCK, need_record_lock},
    };
    if (!tasklets_.empty())
    {
        config_map.insert(std::pair{
            remote_config_keys::TASKLET_ID,
            static_cast<std::size_t>(tasklets_.back()->own_id())});
    }
    return service_config{config_map};
}

local_atst_tree_context::local_atst_tree_context(inner_resources& resources)
    : local_tree_context_base{resources}
{
}

local_atst_context::local_atst_context(
    std::shared_ptr<local_atst_tree_context> tree_ctx,
    service_config const& config)
    : local_async_context_base{tree_ctx, nullptr, true},
      fail_submit_async_{config.get_bool_or_default(
          testing_config_keys::FAIL_SUBMIT_ASYNC, false)},
      resolve_async_delay_{static_cast<int>(config.get_number_or_default(
          testing_config_keys::RESOLVE_ASYNC_DELAY, 0))},
      set_result_delay_{static_cast<int>(config.get_number_or_default(
          testing_config_keys::SET_RESULT_DELAY, 0))}
{
}

local_atst_context::local_atst_context(
    std::shared_ptr<local_atst_tree_context> tree_ctx,
    local_atst_context* parent,
    bool is_req)
    : local_async_context_base{tree_ctx, parent, is_req}
{
}

std::unique_ptr<req_visitor_intf>
local_atst_context::make_ctx_tree_builder()
{
    return std::make_unique<local_atst_context_tree_builder>(*this);
}

void
local_atst_context::set_result(blob result)
{
    if (set_result_delay_ > 0)
    {
        auto& logger{get_tree_context()->get_logger()};
        logger.warn("set_result() forced delay {}ms", set_result_delay_);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(set_result_delay_));
    }
    local_async_context_base::set_result(std::move(result));
}

void
local_atst_context::apply_fail_submit_async()
{
    if (fail_submit_async_)
    {
        auto& logger{get_tree_context()->get_logger()};
        logger.warn("submit_async: forced failure");
        throw remote_error{"submit_async forced failure"};
    }
}

void
local_atst_context::apply_resolve_async_delay()
{
    if (resolve_async_delay_ > 0)
    {
        auto& logger{get_tree_context()->get_logger()};
        logger.warn(
            "resolve_async() forced startup delay {}ms", resolve_async_delay_);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(resolve_async_delay_));
    }
}

local_atst_context_tree_builder::local_atst_context_tree_builder(
    local_atst_context& ctx)
    : local_context_tree_builder_base{ctx}
{
}

local_atst_context_tree_builder::~local_atst_context_tree_builder()
{
}

std::unique_ptr<local_context_tree_builder_base>
local_atst_context_tree_builder::make_sub_builder(
    local_async_context_base& sub_ctx)
{
    auto& my_sub_ctx{static_cast<local_atst_context&>(sub_ctx)};
    return std::make_unique<local_atst_context_tree_builder>(my_sub_ctx);
}

std::shared_ptr<local_async_context_base>
local_atst_context_tree_builder::make_sub_ctx(
    std::shared_ptr<local_tree_context_base> tree_ctx,
    std::size_t ix,
    bool is_req)
{
    auto my_tree_ctx{static_pointer_cast<local_atst_tree_context>(tree_ctx)};
    auto* my_parent{static_cast<local_atst_context*>(&ctx_)};
    return std::make_shared<local_atst_context>(
        my_tree_ctx, my_parent, is_req);
}

proxy_atst_tree_context::proxy_atst_tree_context(
    inner_resources& resources, std::string proxy_name)
    : proxy_async_tree_context_base{resources, std::move(proxy_name)}
{
}

root_proxy_atst_context::root_proxy_atst_context(
    std::shared_ptr<proxy_atst_tree_context> tree_ctx)
    : root_proxy_async_context_base{tree_ctx}
{
}

std::string const&
root_proxy_atst_context::domain_name() const
{
    return the_domain_name;
}

service_config
root_proxy_atst_context::make_config(bool need_record_lock) const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, the_domain_name},
        {remote_config_keys::NEED_RECORD_LOCK, need_record_lock},
    };
    if (fail_submit_async_)
    {
        config_map[testing_config_keys::FAIL_SUBMIT_ASYNC] = true;
    }
    if (resolve_async_delay_ > 0)
    {
        config_map[testing_config_keys::RESOLVE_ASYNC_DELAY]
            = static_cast<std::size_t>(resolve_async_delay_);
    }
    if (set_result_delay_ > 0)
    {
        config_map[testing_config_keys::SET_RESULT_DELAY]
            = static_cast<std::size_t>(set_result_delay_);
    }
    if (!tasklets_.empty())
    {
        config_map.insert(std::pair{
            remote_config_keys::TASKLET_ID,
            static_cast<std::size_t>(tasklets_.back()->own_id())});
    }
    return service_config{config_map};
}

std::unique_ptr<proxy_async_context_base>
root_proxy_atst_context::make_sub_ctx(
    std::shared_ptr<proxy_async_tree_context_base> tree_ctx, bool is_req)
{
    auto my_tree_ctx{static_pointer_cast<proxy_atst_tree_context>(tree_ctx)};
    return std::make_unique<non_root_proxy_atst_context>(my_tree_ctx, is_req);
}

non_root_proxy_atst_context::non_root_proxy_atst_context(
    std::shared_ptr<proxy_atst_tree_context> tree_ctx, bool is_req)
    : non_root_proxy_async_context_base{tree_ctx, is_req}
{
}

non_root_proxy_atst_context::~non_root_proxy_atst_context()
{
}

std::string const&
non_root_proxy_atst_context::domain_name() const
{
    return the_domain_name;
}

service_config
non_root_proxy_atst_context::make_config(bool need_record_lock) const
{
    // Must be called only for a root context
    throw std::logic_error{
        "invalid non_root_proxy_atst_context::make_config() call"};
}

std::unique_ptr<proxy_async_context_base>
non_root_proxy_atst_context::make_sub_ctx(
    std::shared_ptr<proxy_async_tree_context_base> tree_ctx, bool is_req)
{
    auto my_tree_ctx{static_pointer_cast<proxy_atst_tree_context>(tree_ctx)};
    return std::make_unique<non_root_proxy_atst_context>(my_tree_ctx, is_req);
}

atst_context::atst_context(
    inner_resources& resources, std::string const& proxy_name)
    : resources_{resources},
      proxy_name_{proxy_name},
      logger_{ensure_logger("atst")}
{
}

local_async_context_intf&
atst_context::prepare_for_local_resolution()
{
    logger_->info("prepare_for_local_resolution");
    local_tree_ = std::make_shared<local_atst_tree_context>(resources_);
    local_root_
        = std::make_shared<local_atst_context>(local_tree_, nullptr, true);
    register_local_async_ctx(local_root_);
    return *local_root_;
}

local_async_context_intf*
atst_context::get_active_local_root_context()
{
    return local_root_ ? &*local_root_ : nullptr;
}

remote_async_context_intf&
atst_context::prepare_for_remote_resolution()
{
    logger_->info("prepare_for_remote_resolution");
    remote_tree_
        = std::make_shared<proxy_atst_tree_context>(resources_, proxy_name_);
    remote_root_ = std::make_shared<root_proxy_atst_context>(remote_tree_);
    return *remote_root_;
}

remote_async_context_intf*
atst_context::get_active_remote_root_context()
{
    return remote_root_ ? &*remote_root_ : nullptr;
}

local_atst_context&
atst_context::get_local_root()
{
    if (!local_root_)
    {
        throw std::logic_error{"atst_context object has no local_root"};
    }
    return *local_root_;
}

local_atst_context const&
atst_context::get_local_root() const
{
    return const_cast<atst_context*>(this)->get_local_root();
}

root_proxy_atst_context&
atst_context::get_remote_root()
{
    if (!remote_root_)
    {
        throw std::logic_error{"atst_context object has no remote_root"};
    }
    return *remote_root_;
}

root_proxy_atst_context const&
atst_context::get_remote_root() const
{
    return const_cast<atst_context*>(this)->get_remote_root();
}

async_context_intf&
atst_context::get_async_root()
{
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
        throw std::logic_error{"atst_context object has no async root"};
    }
    return *root;
}

async_context_intf const&
atst_context::get_async_root() const
{
    return const_cast<atst_context*>(this)->get_async_root();
}

} // namespace cradle
