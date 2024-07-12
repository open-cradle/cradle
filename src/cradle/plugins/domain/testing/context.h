#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H

#include <atomic>
#include <exception>
#include <future>

#include <spdlog/spdlog.h>

#include <cradle/inner/introspection/tasklet.h>
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
        std::string proxy_name,
        std::optional<root_tasklet_spec> opt_tasklet_spec = std::nullopt);

    // context_intf
    std::string const&
    domain_name() const override;

    // remote_context_intf
    service_config
    make_config(bool need_record_lock) const override;
};
static_assert(ValidFinalContext<testing_request_context>);

/*
 * Context that can be used to asynchronously resolve requests on the local
 * machine.
 *
 * Relates to a single root request, which will be resolved on the local
 * machine.
 */
class root_local_atst_context final : public root_local_async_context_base,
                                      public test_params_context_mixin
{
 public:
    // Allows special configuration for testing purposes
    root_local_atst_context(
        std::unique_ptr<local_tree_context_base> tree_ctx,
        service_config const& config);

    root_local_atst_context(
        std::unique_ptr<local_tree_context_base> tree_ctx,
        tasklet_tracker* tasklet);

    // context_intf
    std::string const&
    domain_name() const override;

    // local_async_context_intf
    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override;

    void
    set_result(blob result) override;

    // test_context_intf
    void
    apply_fail_submit_async() override;

    void
    apply_submit_async_delay() override;

    void
    apply_resolve_async_delay() override;

 private:
    std::unique_ptr<local_tree_context_base> owning_tree_ctx_;
};
static_assert(ValidFinalContext<root_local_atst_context>);

/*
 * Context that can be used to asynchronously resolve requests on the local
 * machine.
 *
 * Relates to a single non-root request, or a non-request argument of such a
 * request, which will be resolved on the local machine.
 */
class non_root_local_atst_context final
    : public non_root_local_async_context_base
{
 public:
    using non_root_local_async_context_base::non_root_local_async_context_base;

    // context_intf
    std::string const&
    domain_name() const override;
};
static_assert(ValidFinalContext<non_root_local_atst_context>);

/*
 * Recursively creates subtrees of non_root_local_atst_context objects, with
 * the same topology as the corresponding request subtree.
 *
 * A non_root_local_atst_context object will be created for each request in the
 * tree, but also for each value: the resolve_request() variant resolving a
 * value requires a context argument, even though it doesn't access it.
 */
class local_atst_context_tree_builder : public local_context_tree_builder_base
{
 public:
    // ctx is the context object corresponding to the request whose arguments
    // will be visited
    local_atst_context_tree_builder(local_async_context_base& ctx);

 private:
    // local_context_tree_builder_base
    std::unique_ptr<local_context_tree_builder_base>
    make_sub_builder(local_async_context_base& sub_ctx) override;

    std::shared_ptr<local_async_context_base>
    make_sub_ctx(
        local_tree_context_base& tree_ctx,
        std::size_t ix,
        bool is_req) override;
};

/*
 * Tree-level context, shared by all root_proxy_atst_context and
 * non_root_proxy_atst_context objects in the same context tree
 * (relating to the same root request), in the "testing" domain;
 * owned by the root_proxy_atst_context object.
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
class root_proxy_atst_context final : public root_proxy_async_context_base,
                                      public test_params_context_mixin
{
 public:
    root_proxy_atst_context(
        std::unique_ptr<proxy_atst_tree_context> tree_ctx,
        tasklet_tracker* tasklet);

    ~root_proxy_atst_context();

    // context_intf
    std::string const&
    domain_name() const override;

    // remote_context_intf
    service_config
    make_config(bool need_record_lock) const override;

 private:
    std::unique_ptr<proxy_atst_tree_context> owning_tree_ctx_;
    tasklet_tracker* tasklet_;

    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        proxy_async_tree_context_base& tree_ctx, bool is_req) override;
};
static_assert(ValidFinalContext<root_proxy_atst_context>);

/*
 * Context that can be used to asynchronously resolve non-root requests in the
 * "testing" domain, on a remote machine.
 */
class non_root_proxy_atst_context final
    : public non_root_proxy_async_context_base
{
 public:
    non_root_proxy_atst_context(
        proxy_atst_tree_context& tree_ctx, bool is_req);

    // context_intf
    std::string const&
    domain_name() const override;

    // remote_context_intf
    service_config
    make_config(bool need_record_lock) const override;

 private:
    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        proxy_async_tree_context_base& tree_ctx, bool is_req) override;
};
static_assert(ValidFinalContext<non_root_proxy_atst_context>);

// Async context that can be used multiple times for resolving a request.
// Each resolve_request() leads to an active request tree for that resolution;
// the tree has a root context that is either local or remote.
// The atst_context functionality is limited: it can be passed to
// resolve_request(), and a root context object can be retrieved for additional
// functionality.
// TODO if async context used for several subsequent resolutions, (query)
// functions may refer to the wrong resolution.
class atst_context final : public root_local_async_context_intf,
                           public remote_async_context_intf,
                           public local_async_ctx_owner_intf,
                           public remote_async_ctx_owner_intf,
                           public test_params_context_mixin
{
 public:
    atst_context(
        inner_resources& resources,
        std::string proxy_name = "",
        std::optional<root_tasklet_spec> opt_tasklet_spec = std::nullopt);

    // context_intf
    inner_resources&
    get_resources() override
    {
        return resources_;
    }

    bool
    remotely() const override
    {
        return !proxy_name_.empty();
    }

    bool
    is_async() const override
    {
        return true;
    }

    std::string const&
    domain_name() const override;

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override
    {
        return get_async_root().schedule_after(delay);
    }

    // async_context_intf
    async_id
    get_id() const override
    {
        return get_async_root().get_id();
    }

    bool
    is_req() const override
    {
        return get_async_root().is_req();
    }

    std::size_t
    get_num_subs() const override
    {
        return get_async_root().get_num_subs();
    }

    async_context_intf&
    get_sub(std::size_t ix) override
    {
        return get_async_root().get_sub(ix);
    }

    cppcoro::task<async_status>
    get_status_coro() override
    {
        return get_async_root().get_status_coro();
    }

    cppcoro::task<void>
    request_cancellation_coro() override
    {
        return get_async_root().request_cancellation_coro();
    }

    // local_async_context_intf
    std::size_t
    get_local_num_subs() const override
    {
        return get_local_root().get_local_num_subs();
    }

    local_async_context_base&
    get_local_sub(std::size_t ix) override
    {
        return get_local_root().get_local_sub(ix);
    }

    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override
    {
        return get_local_root().make_ctx_tree_builder();
    }

    cppcoro::task<void>
    reschedule_if_opportune() override
    {
        return get_local_root().reschedule_if_opportune();
    }

    async_status
    get_status() override
    {
        return get_local_root().get_status();
    }

    std::string
    get_error_message() override
    {
        return get_local_root().get_error_message();
    }

    void
    update_status(async_status status) override
    {
        get_local_root().update_status(status);
    }

    void
    update_status_error(std::string const& errmsg) override
    {
        get_local_root().update_status_error(errmsg);
    }

    void
    using_result() override
    {
        get_local_root().using_result();
    }

    void
    set_result(blob result) override
    {
        get_local_root().set_result(std::move(result));
    }

    blob
    get_result() override
    {
        return get_local_root().get_result();
    }

    void
    set_cache_record_id(remote_cache_record_id record_id) override
    {
        get_local_root().set_cache_record_id(record_id);
    }

    remote_cache_record_id
    get_cache_record_id() const override
    {
        return get_local_root().get_cache_record_id();
    }

    void
    request_cancellation() override
    {
        get_local_root().request_cancellation();
    }

    bool
    is_cancellation_requested() const noexcept override
    {
        return get_local_root().is_cancellation_requested();
    }

    void
    throw_async_cancelled() const override
    {
        get_local_root().throw_async_cancelled();
    }

    void
    set_delegate(std::shared_ptr<async_context_intf> delegate) override
    {
        get_local_root().set_delegate(std::move(delegate));
    }

    std::shared_ptr<async_context_intf>
    get_delegate() override
    {
        return get_local_root().get_delegate();
    }

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override
    {
        return get_local_root().make_data_owner(size, use_shared_memory);
    }

    void
    track_blob_file_writers() override
    {
        get_local_root().track_blob_file_writers();
    }

    void
    on_value_complete() override
    {
        get_local_root().on_value_complete();
    }

    // remote_context_intf
    remote_proxy&
    get_proxy() const override
    {
        return get_remote_root().get_proxy();
    }

    service_config
    make_config(bool need_record_lock) const override
    {
        return get_remote_root().make_config(need_record_lock);
    }

    // remote_async_context_intf
    void
    set_remote_id(async_id remote_id) override
    {
        get_remote_root().set_remote_id(remote_id);
    }

    void
    fail_remote_id() noexcept override
    {
        get_remote_root().fail_remote_id();
    }

    async_id
    get_remote_id() override
    {
        return get_remote_root().get_remote_id();
    }

    // local_async_ctx_owner_intf
    root_local_async_context_intf&
    prepare_for_local_resolution() override;

    // remote_async_ctx_owner_intf
    remote_async_context_intf&
    prepare_for_remote_resolution() override;

    // Returns the root context object for the current resolution, whether
    // local or remote. Blocks until the object is available; it becomes so
    // in resolve_request() or co_await resolve_request() on this context.
    async_context_intf&
    get_async_root();

    async_context_intf const&
    get_async_root() const;

    // Returns the root context object for the current remote resolution.
    // Blocks until the object is available; it becomes so in
    // resolve_request() or co_await resolve_request() on this context.
    root_proxy_atst_context&
    get_remote_root();

    root_proxy_atst_context const&
    get_remote_root() const;

 private:
    inner_resources& resources_;
    std::string proxy_name_;
    std::optional<root_tasklet_spec> opt_tasklet_spec_;
    std::shared_ptr<spdlog::logger> logger_;

    std::atomic<bool> prepared_{false};
    // Promise/future associated with prepare_for_..._resolution()
    std::promise<void> preparation_promise_;
    std::shared_future<void> preparation_future_;
    // local_root_ used only when proxy_name_ is empty;
    // local_root_ ownership shared between this object and async db
    // TODO local_root_, remote_root_ accessed from different threads
    std::shared_ptr<root_local_atst_context> local_root_;
    // remote_root_ used only when proxy_name_ is non-empty;
    // remote_root_ exclusively owned by this object
    std::unique_ptr<root_proxy_atst_context> remote_root_;

    void
    on_preparation_finished();

    void
    on_preparation_failed(std::exception const& exception_to_throw);

    void
    wait_until_prepared();

    root_local_async_context_base&
    get_local_root();

    root_local_async_context_base const&
    get_local_root() const;
};
static_assert(ValidFinalContext<atst_context>);

} // namespace cradle

#endif
