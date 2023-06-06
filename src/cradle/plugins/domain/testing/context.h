#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H

#include <cradle/inner/requests/context_base.h>

/*
 * This file defines a collection of concrete context classes for the "testing"
 * domain. These classes fill in gaps in the context bases classes, mostly by
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
        bool remotely,
        std::string proxy_name);

    // remote_context_intf
    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<local_context_intf>
    local_clone() const override;

 private:
    std::string domain_name_{"testing"};
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
 * Context that can be used to asynchronously resolve requests in the "testing"
 * domain, on the local machine.
 *
 * Relates to a single request, or a non-request argument of such a request,
 * which will be resolved on the local machine.
 */
class local_atst_context final : public local_async_context_base
{
 public:
    local_atst_context(
        std::shared_ptr<local_atst_tree_context> tree_ctx,
        local_atst_context* parent,
        bool is_req);

    // local_async_context_intf
    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override;
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
 * context tree (relating to the same root request).
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
 * "testing" domain on a remote machine.
 */
class root_proxy_atst_context final : public root_proxy_async_context_base
{
 public:
    root_proxy_atst_context(std::shared_ptr<proxy_atst_tree_context> tree_ctx);

    // remote_context_intf
    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

 private:
    std::string domain_name_{"testing"};

    // proxy_async_context_base
    std::shared_ptr<local_async_context_base>
    make_local_clone() const override;

    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx,
        bool is_req) override;
};
static_assert(ValidContext<root_proxy_atst_context>);

/*
 * Context that can be used to asynchronously resolve non-root requests in the
 * "testing" domain on a remote machine.
 */
class non_root_proxy_atst_context final
    : public non_root_proxy_async_context_base
{
 public:
    non_root_proxy_atst_context(
        std::shared_ptr<proxy_atst_tree_context> tree_ctx, bool is_req);

    ~non_root_proxy_atst_context();

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

 private:
    std::string domain_name_{"testing"};

    // proxy_async_context_base
    std::shared_ptr<local_async_context_base>
    make_local_clone() const override;

    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx,
        bool is_req) override;
};
static_assert(ValidContext<non_root_proxy_atst_context>);

} // namespace cradle

#endif
