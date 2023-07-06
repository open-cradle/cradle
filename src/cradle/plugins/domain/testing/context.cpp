#include <chrono>
#include <thread>

#include <cradle/inner/remote/config.h>
#include <cradle/plugins/domain/testing/config.h>
#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

std::string const the_domain_name{"testing"};

testing_request_context::testing_request_context(
    inner_resources& resources,
    tasklet_tracker* tasklet,
    bool remotely,
    std::string proxy_name)
    : sync_context_base{resources, tasklet, remotely, std::move(proxy_name)}
{
}

std::string const&
testing_request_context::domain_name() const
{
    return the_domain_name;
}

service_config
testing_request_context::make_config() const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, the_domain_name},
    };
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
root_proxy_atst_context::make_config() const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, the_domain_name},
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
non_root_proxy_atst_context::make_config() const
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

} // namespace cradle
