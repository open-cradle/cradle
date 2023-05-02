#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H

#include <atomic>
#include <memory>
#include <vector>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

/*
 * A context that can be used to synchronously resolve requests in the
 * "testing" domain.
 * It offers all context features other than the asynchronous functionality
 * (i.e., implements all context interfaces other than async_context_intf).
 */
class testing_request_context final : public local_context_intf,
                                      public remote_context_intf,
                                      public sync_context_intf,
                                      public cached_introspective_context_intf
{
 public:
    testing_request_context(
        inner_resources& resources,
        tasklet_tracker* tasklet,
        bool remotely = false);

    // context_intf
    bool
    remotely() const override
    {
        return remotely_;
    }

    bool
    is_async() const override
    {
        return false;
    }

    // remote_context_intf
    std::string const&
    proxy_name() const override;

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<local_context_intf>
    local_clone() const override;

    // cached_context_intf
    inner_resources&
    get_resources() override
    {
        return resources_;
    }

    // introspective_context_intf
    tasklet_tracker*
    get_tasklet() override;

    void
    push_tasklet(tasklet_tracker* tasklet) override;

    void
    pop_tasklet() override;

    // Other
    void
    proxy_name(std::string const& name);

 private:
    inner_resources& resources_;
    bool remotely_;
    std::string proxy_name_;
    std::string domain_name_{"testing"};
    std::vector<tasklet_tracker*> tasklets_;
};

static_assert(ValidContext<testing_request_context>);

/*
 * Tree-level context, shared by all local_atst_context objects in the same
 * context tree (relating to the same root request).
 *
 * In particular, it owns a cppcoro::cancellation_source object, which is
 * shared by all contexts in the tree.
 */
class local_atst_tree_context
{
 public:
    local_atst_tree_context(inner_resources& resources);

    // Once created, these objects should not be moved.
    local_atst_tree_context(local_atst_tree_context const&) = delete;
    void
    operator=(local_atst_tree_context const&)
        = delete;
    local_atst_tree_context(local_atst_tree_context&&) = delete;
    void
    operator=(local_atst_tree_context&&)
        = delete;

    inner_resources&
    get_resources()
    {
        return resources_;
    }

    void
    request_cancellation()
    {
        csource_.request_cancellation();
    }

    cppcoro::cancellation_token
    get_cancellation_token()
    {
        return ctoken_;
    }

    spdlog::logger&
    get_logger() noexcept
    {
        return *logger_;
    }

 private:
    inner_resources& resources_;
    cppcoro::cancellation_source csource_;
    cppcoro::cancellation_token ctoken_;
    std::shared_ptr<spdlog::logger> logger_;
};

/*
 * Context that can be used to asynchronously resolve requests in the "atst"
 * domain, on the local machine.
 * Relates to a single request, or a non-request argument of such a request,
 * which will be resolved on the local machine.
 */
class local_atst_context final : public local_async_context_intf,
                                 public cached_introspective_context_intf
{
 public:
    local_atst_context(
        std::shared_ptr<local_atst_tree_context> tree_ctx,
        local_atst_context* parent,
        bool is_req);

    // Once created, these objects should not be moved.
    local_atst_context(local_atst_context const&) = delete;
    void
    operator=(local_atst_context const&)
        = delete;
    local_atst_context(local_atst_context&&) = delete;
    void
    operator=(local_atst_context&&)
        = delete;

    // context_intf
    bool
    remotely() const override
    {
        return false;
    }

    bool
    is_async() const override
    {
        return true;
    }

    // async_context_intf
    async_id
    get_id() const override
    {
        return id_;
    }

    bool
    is_req() const override
    {
        return is_req_;
    }

    std::size_t
    get_num_subs() const override
    {
        return subs_.size();
    }

    async_context_intf&
    get_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    cppcoro::task<async_status>
    get_status_coro() override;

    cppcoro::task<void>
    request_cancellation_coro() override;

    // local_async_context_intf
    local_atst_context&
    get_local_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override;

    cppcoro::task<void>
    reschedule_if_opportune() override;

    async_status
    get_status() override;

    std::string
    get_error_message() override
    {
        return errmsg_;
    }

    void
    update_status(async_status status) override;

    void
    update_status_error(std::string const& errmsg) override;

    void
    set_result(blob result) override;

    blob
    get_result() override;

    void
    request_cancellation() override
    {
        tree_ctx_->request_cancellation();
    }

    bool
    is_cancellation_requested() const noexcept override;

    void
    throw_if_cancellation_requested() const override;

    // cached_context_intf
    inner_resources&
    get_resources() override
    {
        return tree_ctx_->get_resources();
    }

    // introspective_context_intf
    // TODO add support for introspection
    tasklet_tracker*
    get_tasklet() override
    {
        return nullptr;
    }

    void
    push_tasklet(tasklet_tracker* tasklet) override
    {
        throw not_implemented_error();
    }

    void
    pop_tasklet() override
    {
        throw not_implemented_error();
    }

    // Other
    void
    add_sub(std::size_t ix, std::shared_ptr<local_atst_context> sub);

    std::shared_ptr<local_atst_tree_context>
    get_tree_context()
    {
        return tree_ctx_;
    }

 private:
    std::shared_ptr<local_atst_tree_context> tree_ctx_;
    local_atst_context* parent_;
    bool is_req_;
    async_id id_;
    async_status status_;
    std::string errmsg_;
    blob result_;
    // Using shared_ptr ensures that local_atst_context objects are not
    // relocated during tree build-up / visit.
    // It cannot be unique_ptr because there can be two owners: the parent
    // context, and the async_db.
    std::vector<std::shared_ptr<local_atst_context>> subs_;
    std::atomic<int> num_subs_not_running_;

    bool
    decide_reschedule_sub();
};

static_assert(ValidContext<local_atst_context>);

/*
 * Creates a local_atst_context object for the root node in a context tree.
 * Child objects will be added by class local_atst_context_tree_builder.
 */
std::shared_ptr<local_atst_context>
make_root_local_atst_context(
    std::shared_ptr<local_atst_tree_context> tree_ctx);

/*
 * Recursively creates subtrees for a local_atst_context tree node, with the
 * same topology as the corresponding request subtree.
 */
class local_atst_context_tree_builder : public req_visitor_intf
{
 public:
    local_atst_context_tree_builder(local_atst_context& ctx);

    void
    visit_val_arg(std::size_t ix) override;

    std::unique_ptr<req_visitor_intf>
    visit_req_arg(std::size_t ix) override;

 private:
    local_atst_context& ctx_;
};

/*
 * Creates a tree of local_atst_context objects, reflecting the structure of
 * the request tree of which `root_req' is the root request.
 */
// TODO Req must have visit()
template<typename Req>
std::shared_ptr<local_atst_context>
make_local_async_ctx_tree(
    std::shared_ptr<local_atst_tree_context> tree_ctx, Req const& root_req)
{
    auto root_ctx{make_root_local_atst_context(std::move(tree_ctx))};
    local_atst_context_tree_builder builder{*root_ctx};
    root_req.visit(builder);
    return root_ctx;
}

/*
 * Tree-level context, shared by all proxy_atst_context objects in the same
 * context tree (relating to the same root request).
 *
 * In particular, it owns a remote_proxy reference shared by all context
 * objects in the tree.
 */
class proxy_atst_tree_context
{
 public:
    proxy_atst_tree_context(
        inner_resources& resources, std::string const& proxy_name);

    inner_resources&
    get_resources() const
    {
        return resources_;
    }

    std::string const&
    get_proxy_name() const
    {
        return proxy_name_;
    }

    remote_proxy&
    get_proxy() const
    {
        return proxy_;
    }

    spdlog::logger&
    get_logger() const noexcept
    {
        return *logger_;
    }

 private:
    inner_resources& resources_;
    std::string proxy_name_;
    remote_proxy& proxy_;
    std::shared_ptr<spdlog::logger> logger_;
};

/*
 * Context that can be used to asynchronously resolve requests in the "atst"
 * domain, on a remote machine.
 * It acts as a proxy for a local_atst_context object on a remote server.
 */
class proxy_atst_context final : public remote_async_context_intf
{
 public:
    proxy_atst_context(
        std::shared_ptr<proxy_atst_tree_context> tree_ctx,
        bool is_root,
        bool is_req);

    ~proxy_atst_context();

    // Once created, these objects should not be moved.
    proxy_atst_context(proxy_atst_context const&) = delete;
    void
    operator=(proxy_atst_context const&)
        = delete;
    proxy_atst_context(proxy_atst_context&&) = delete;
    void
    operator=(proxy_atst_context&&)
        = delete;

    // context_intf
    bool
    remotely() const override
    {
        return true;
    }

    bool
    is_async() const override
    {
        return true;
    }

    // remote_context_intf
    std::string const&
    proxy_name() const override
    {
        return tree_ctx_->get_proxy_name();
    }

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<local_context_intf>
    local_clone() const override
    {
        return make_local_clone();
    }

    // async_context_intf
    async_id
    get_id() const override
    {
        return id_;
    }

    bool
    is_req() const override
    {
        return is_req_;
    }

    std::size_t
    get_num_subs() const override
    {
        return subs_.size();
    }

    async_context_intf&
    get_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    cppcoro::task<async_status>
    get_status_coro() override;

    cppcoro::task<void>
    request_cancellation_coro() override;

    // remote_async_context_intf
    std::shared_ptr<local_async_context_intf>
    local_async_clone() const override
    {
        return make_local_clone();
    }

    remote_async_context_intf&
    add_sub(bool is_req) override;

    proxy_atst_context&
    get_remote_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    void
    set_remote_id(async_id remote_id) override
    {
        remote_id_ = remote_id;
    }

    async_id
    get_remote_id() override
    {
        return remote_id_;
    }

    bool
    cancellation_pending() override
    {
        return cancellation_pending_;
    }

 private:
    std::string domain_name_{"atst"};
    std::shared_ptr<proxy_atst_tree_context> tree_ctx_;
    bool is_root_;
    bool is_req_;
    async_id id_;
    async_id remote_id_{NO_ASYNC_ID};
    bool cancellation_pending_{false};
    // Using unique_ptr ensures that proxy_atst_context objects are not
    // relocated during tree build-up / visit.
    std::vector<std::unique_ptr<proxy_atst_context>> subs_;

    remote_proxy&
    get_proxy()
    {
        return tree_ctx_->get_proxy();
    }

    std::shared_ptr<local_atst_context>
    make_local_clone() const;
};

static_assert(ValidContext<proxy_atst_context>);

/*
 * Creates a proxy_atst_context object for the root node in a context tree.
 * Subobjects are to be added later.
 */
inline proxy_atst_context
make_root_proxy_atst_context(std::shared_ptr<proxy_atst_tree_context> tree_ctx)
{
    return proxy_atst_context{std::move(tree_ctx), true, true};
}

} // namespace cradle

#endif
