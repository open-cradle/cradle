#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H

#include <cradle/inner/requests/context_base.h>

/*
 * This file defines a collection of concrete context classes, in the "testing"
 * domain.
 * These classes fill in gaps in the context bases classes, mostly by
 * defining a number of factory functions (specific to the concrete classes).
 */

namespace cradle {

/*
 * A context that can be used to synchronously resolve requests in the
 * "testing" domain.
 * It offers all context features other than the asynchronous functionality
 * (i.e., implements all context interfaces other than async_context_intf).
 */
class testing_request_context final : public sync_context_base
{
 public:
    testing_request_context(
        inner_resources& resources,
        tasklet_tracker* tasklet,
        std::string proxy_name);

    // remote_context_intf
    std::string const&
    domain_name() const override;

    service_config
    make_config() const override;
};
static_assert(ValidContext<testing_request_context>);

/*
 * Tree-level context, shared by all local_atst_context objects in the same
 * context tree (relating to the same root request).
 *
 * Note that an object of this class must not be re-used across multiple
 * context trees.
 */
class local_atst_tree_context : public local_tree_context_base
{
 public:
    local_atst_tree_context(inner_resources& resources);
};

/*
 * Context that can be used to asynchronously resolve requests on the local
 * machine.
 *
 * Relates to a single request, or a non-request argument of such a request,
 * which will be resolved on the local machine.
 */
class local_atst_context final : public local_async_context_base
{
 public:
    // Constructor called from testing_domain::make_local_async_context()
    local_atst_context(
        std::shared_ptr<local_atst_tree_context> tree_ctx,
        service_config const& config);

    // Other-purposes constructor
    local_atst_context(
        std::shared_ptr<local_atst_tree_context> tree_ctx,
        local_atst_context* parent,
        bool is_req);

    // local_async_context_intf
    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override;

    void
    set_result(blob result) override;

    // test_context_intf
    void
    apply_fail_submit_async() override;

    void
    apply_resolve_async_delay() override;

 private:
    bool fail_submit_async_{false};
    int resolve_async_delay_{0};
    int set_result_delay_{0};
};
static_assert(ValidContext<local_atst_context>);

/*
 * Recursively creates subtrees of local_atst_context objects, with the
 * same topology as the corresponding request subtree.
 *
 * A local_atst_context object will be created for each request in the tree,
 * but also for each value: the resolve_request() variant resolving a value
 * requires a context argument, even though it doesn't access it.
 */
class local_atst_context_tree_builder : public local_context_tree_builder_base
{
 public:
    // ctx is the context object corresponding to the request whose arguments
    // will be visited
    local_atst_context_tree_builder(local_atst_context& ctx);

    ~local_atst_context_tree_builder();

 private:
    // local_context_tree_builder_base
    std::unique_ptr<local_context_tree_builder_base>
    make_sub_builder(local_async_context_base& sub_ctx) override;

    std::shared_ptr<local_async_context_base>
    make_sub_ctx(
        std::shared_ptr<local_tree_context_base> tree_ctx,
        std::size_t ix,
        bool is_req) override;
};

/*
 * Creates a tree of local_atst_context objects, reflecting the structure of
 * the request tree of which `root_req' is the root request.
 */
template<VisitableRequest Req>
std::shared_ptr<local_atst_context>
make_local_async_ctx_tree(
    std::shared_ptr<local_atst_tree_context> tree_ctx, Req const& root_req)
{
    auto root_ctx{
        std::make_shared<local_atst_context>(tree_ctx, nullptr, true)};
    register_local_async_ctx(root_ctx);
    local_atst_context_tree_builder builder{*root_ctx};
    root_req.accept(builder);
    return root_ctx;
}

/*
 * Tree-level context, shared by all proxy_atst_context objects in the same
 * context tree (relating to the same root request), in the "testing" domain.
 *
 * Note that an object of this class should not be re-used across multiple
 * context trees.
 */
class proxy_atst_tree_context : public proxy_async_tree_context_base
{
 public:
    proxy_atst_tree_context(
        inner_resources& resources, std::string proxy_name);
};

/*
 * Context that can be used to asynchronously resolve root requests in the
 * "testing" domain, on a remote machine.
 */
class root_proxy_atst_context final : public root_proxy_async_context_base
{
 public:
    root_proxy_atst_context(std::shared_ptr<proxy_atst_tree_context> tree_ctx);

    // remote_context_intf
    std::string const&
    domain_name() const override;

    service_config
    make_config() const override;

    // Other

    // Causes submit_async to fail on the remote
    void
    fail_submit_async()
    {
        fail_submit_async_ = true;
    }

    // Sets the delay (in ms) that a resolve_async operation / thread will wait
    // after starting.
    // By extending / aggerating the existing short delay, the corresponding
    // race condition becomes reproducible and can be checked in a unit test.
    void
    set_resolve_async_delay(int delay)
    {
        resolve_async_delay_ = delay;
    }

    // Sets the delay (in ms) that a set_result() call will wait before
    // actually setting the result.
    // By extending / aggerating the existing short delay, the corresponding
    // race condition becomes reproducible and can be checked in a unit test.
    void
    set_set_result_delay(int delay)
    {
        set_result_delay_ = delay;
    }

 private:
    bool fail_submit_async_{false};
    int resolve_async_delay_{0};
    int set_result_delay_{0};

    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx,
        bool is_req) override;
};
static_assert(ValidContext<root_proxy_atst_context>);

/*
 * Context that can be used to asynchronously resolve non-root requests in the
 * "testing" domain, on a remote machine.
 */
class non_root_proxy_atst_context final
    : public non_root_proxy_async_context_base
{
 public:
    non_root_proxy_atst_context(
        std::shared_ptr<proxy_atst_tree_context> tree_ctx, bool is_req);

    ~non_root_proxy_atst_context();

    // remote_context_intf
    std::string const&
    domain_name() const override;

    service_config
    make_config() const override;

 private:
    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx,
        bool is_req) override;
};
static_assert(ValidContext<non_root_proxy_atst_context>);

} // namespace cradle

#endif
