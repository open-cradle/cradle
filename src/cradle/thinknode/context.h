#ifndef CRADLE_THINKNODE_CONTEXT_H
#define CRADLE_THINKNODE_CONTEXT_H

#include <cradle/inner/requests/context_base.h>
#include <cradle/inner/service/config.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/thinknode/types.hpp>
#include <memory>

namespace cradle {

struct thinknode_request_context_base : public local_context_intf
{
    thinknode_session session;
};

struct thinknode_request_context final : public thinknode_request_context_base,
                                         public sync_context_base
{
    service_core& service;

    // Constructor called from thinknode_domain::make_local_sync_context()
    thinknode_request_context(
        service_core& service, service_config const& config);

    // Other-purposes constructor
    thinknode_request_context(
        service_core& service,
        thinknode_session session,
        tasklet_tracker* tasklet,
        std::string proxy_name);

    ~thinknode_request_context();

    // context_intf
    std::string const&
    domain_name() const override;

    // remote_context_intf
    service_config
    make_config(bool need_record_lock) const override;

    // Other
    std::string const&
    api_url() const
    {
        return session.api_url;
    }
};
static_assert(ValidFinalContext<thinknode_request_context>);

struct root_local_async_thinknode_context final
    : public thinknode_request_context_base,
      public root_local_async_context_base
{
    // service_core& service;
    thinknode_session session;

    root_local_async_thinknode_context(
        std::unique_ptr<local_tree_context_base> tree_ctx,
        service_config const& config);

    root_local_async_thinknode_context(
        std::unique_ptr<local_tree_context_base> tree_ctx,
        thinknode_session session);

    ~root_local_async_thinknode_context()
    {
    }

    // context_intf
    std::string const&
    domain_name() const override;

    // local_async_context_intf
    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override;

    void
    apply_submit_async_delay() override
    {
    }

    // Other
    std::string const&
    api_url() const
    {
        return session.api_url;
    }

 private:
    std::unique_ptr<local_tree_context_base> owning_tree_ctx_;
};
static_assert(ValidFinalContext<root_local_async_thinknode_context>);

/*
 * Context that can be used to asynchronously resolve requests on the local
 * machine.
 *
 * Relates to a single non-root request, or a non-request argument of such a
 * request, which will be resolved on the local machine.
 */
class non_root_local_async_thinknode_context final
    : public non_root_local_async_context_base
{
 public:
    using non_root_local_async_context_base::non_root_local_async_context_base;

    // context_intf
    std::string const&
    domain_name() const override;
};
static_assert(ValidFinalContext<non_root_local_async_thinknode_context>);

/*
 * Recursively creates subtrees of non_root_local_atst_context objects, with
 * the same topology as the corresponding request subtree.
 *
 * A non_root_local_atst_context object will be created for each request in the
 * tree, but also for each value: the resolve_request() variant resolving a
 * value requires a context argument, even though it doesn't access it.
 */
class local_async_thinknode_context_tree_builder
    : public local_context_tree_builder_base
{
 public:
    // ctx is the context object corresponding to the request whose arguments
    // will be visited
    local_async_thinknode_context_tree_builder(local_async_context_base& ctx);

 private:
    // local_context_tree_builder_base
    std::unique_ptr<local_context_tree_builder_base>
    make_sub_builder(local_async_context_base& sub_ctx) override;

    std::shared_ptr<local_async_context_base>
    make_sub_ctx(
        local_tree_context_base& tree_ctx,
        std::size_t ix,
        bool is_req,
        std::unique_ptr<request_essentials> essentials) override;
};

} // namespace cradle

#endif
