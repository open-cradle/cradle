#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

testing_request_context::testing_request_context(
    inner_resources& resources,
    tasklet_tracker* tasklet,
    bool remotely,
    std::string proxy_name)
    : sync_context_base{resources, tasklet, remotely, std::move(proxy_name)}
{
}

std::shared_ptr<local_context_intf>
testing_request_context::local_clone() const
{
    // TODO tasklets should continue in the clone
    return std::make_shared<testing_request_context>(
        resources_, nullptr, false, "");
}

local_atst_tree_context::local_atst_tree_context(inner_resources& resources)
    : local_tree_context_base{resources}
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

std::shared_ptr<local_async_context_base>
root_proxy_atst_context::make_local_clone() const
{
    return std::make_shared<local_atst_context>(
        std::make_shared<local_atst_tree_context>(tree_ctx_->get_resources()),
        nullptr,
        is_req());
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

std::shared_ptr<local_async_context_base>
non_root_proxy_atst_context::make_local_clone() const
{
    // Will be called only for a root context
    throw std::logic_error{
        "invalid non_root_proxy_atst_context::make_local_clone() call"};
}

std::unique_ptr<proxy_async_context_base>
non_root_proxy_atst_context::make_sub_ctx(
    std::shared_ptr<proxy_async_tree_context_base> tree_ctx, bool is_req)
{
    auto my_tree_ctx{static_pointer_cast<proxy_atst_tree_context>(tree_ctx)};
    return std::make_unique<non_root_proxy_atst_context>(my_tree_ctx, is_req);
}

} // namespace cradle
