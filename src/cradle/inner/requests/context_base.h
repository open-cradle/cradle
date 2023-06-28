#ifndef CRADLE_INNER_REQUEST_CONTEXT_BASE_H
#define CRADLE_INNER_REQUEST_CONTEXT_BASE_H

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>

/*
 * This file defines a collection of context classes (and a few others) that
 * implement most of the functions in the context interfaces. These classes are
 * intended to be used as base classes for concrete (domain-specific) context
 * classes; so shouldn't be instantiated from client code even if they happen
 * not to be abstract.
 *
 * The concrete classes should not need to contain much more than a few factory
 * functions.
 */

namespace cradle {

/*
 * An abstract base class that can be used to synchronously resolve requests.
 * It offers all context features other than the asynchronous functionality
 * (i.e., implements all context interfaces other than async_context_intf).
 */
class sync_context_base : public local_context_intf,
                          public remote_context_intf,
                          public sync_context_intf,
                          public caching_context_intf,
                          public introspective_context_intf
{
 public:
    sync_context_base(
        inner_resources& resources,
        tasklet_tracker* tasklet,
        bool remotely,
        std::string proxy_name);

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

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override;

    void
    on_value_complete() override;

    // remote_context_intf
    std::string const&
    proxy_name() const override
    {
        return proxy_name_;
    }

    remote_proxy&
    get_proxy() const override;

    virtual std::string const&
    domain_name() const override
        = 0;

    virtual service_config
    make_config() const override
        = 0;

    // caching_context_intf
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

 protected:
    inner_resources& resources_;
    bool remotely_;
    std::string proxy_name_;
    std::vector<tasklet_tracker*> tasklets_;
    // The blob_file_writer objects allocated during the resolution of requests
    // associated with this context. Most useful if the context is used for
    // resolving a single request only.
    std::vector<std::shared_ptr<blob_file_writer>> blob_file_writers_;
};

/*
 * Tree-level context, shared by all local_async_context_base objects in the
 * same context tree (relating to the same root request).
 *
 * In particular, it owns a cppcoro::cancellation_source object, which is
 * shared by all contexts in the tree.
 *
 * Note that an object of this class must not be re-used across multiple
 * context trees.
 */
class local_tree_context_base
{
 public:
    local_tree_context_base(inner_resources& resources);

    // Once created, these objects should not be moved.
    local_tree_context_base(local_tree_context_base const&) = delete;
    void
    operator=(local_tree_context_base const&)
        = delete;
    local_tree_context_base(local_tree_context_base&&) = delete;
    void
    operator=(local_tree_context_base&&)
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
 * Context that can be used to asynchronously resolve requests, on the local
 * machine.
 *
 * Relates to a single request, or a non-request argument of such a request,
 * which will be resolved on the local machine.
 */
class local_async_context_base : public local_async_context_intf,
                                 public caching_context_intf,
                                 public introspective_context_intf
{
 public:
    local_async_context_base(
        std::shared_ptr<local_tree_context_base> tree_ctx,
        local_async_context_base* parent,
        bool is_req);

    // Once created, these objects should not be moved.
    local_async_context_base(local_async_context_base const&) = delete;
    void
    operator=(local_async_context_base const&)
        = delete;
    local_async_context_base(local_async_context_base&&) = delete;
    void
    operator=(local_async_context_base&&)
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

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override;

    void
    on_value_complete() override;

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
    std::size_t
    get_local_num_subs() const override
    {
        return subs_.size();
    }

    local_async_context_base&
    get_local_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    virtual std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override = 0;

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
    using_result() override;

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

    // caching_context_intf
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
    add_sub(std::size_t ix, std::shared_ptr<local_async_context_base> sub);

    std::shared_ptr<local_tree_context_base>
    get_tree_context()
    {
        return tree_ctx_;
    }

 private:
    std::shared_ptr<local_tree_context_base> tree_ctx_;
    local_async_context_base* parent_;
    bool is_req_;
    async_id id_;
    async_status status_;
    std::string errmsg_;
    bool using_result_{false};
    blob result_;
    // Using shared_ptr ensures that local_async_context_base objects are not
    // relocated during tree build-up / visit.
    // It cannot be unique_ptr because there can be two owners: the parent
    // context, and the async_db.
    std::vector<std::shared_ptr<local_async_context_base>> subs_;
    std::atomic<int> num_subs_not_running_;
    std::vector<std::shared_ptr<blob_file_writer>> blob_file_writers_;

    void
    check_set_get_result_precondition(bool is_get_result);

    bool
    decide_reschedule_sub();
};

/*
 * Recursively creates subtrees of local_async_context_base objects, with the
 * same topology as the corresponding request subtree.
 *
 * A local_async_context_base object will be created for each request in the
 * tree, but also for each value: the resolve_request() variant resolving a
 * value requires a context argument, even though it doesn't access it.
 */
class local_context_tree_builder_base : public req_visitor_intf
{
 public:
    // ctx is the context object corresponding to the request whose arguments
    // will be visited
    local_context_tree_builder_base(local_async_context_base& ctx);

    virtual ~local_context_tree_builder_base();

    void
    visit_val_arg(std::size_t ix) override;

    std::unique_ptr<req_visitor_intf>
    visit_req_arg(std::size_t ix) override;

 protected:
    local_async_context_base& ctx_;

    virtual std::unique_ptr<local_context_tree_builder_base>
    make_sub_builder(local_async_context_base& sub_ctx) = 0;

    std::shared_ptr<local_async_context_base>
    make_sub_ctx(std::size_t ix, bool is_req);

    virtual std::shared_ptr<local_async_context_base>
    make_sub_ctx(
        std::shared_ptr<local_tree_context_base> tree_ctx,
        std::size_t ix,
        bool is_req)
        = 0;
};

/*
 * Tree-level context, shared by all proxy_async_context_base objects in the
 * same context tree (relating to the same root request).
 *
 * In particular, it owns a remote_proxy reference shared by all context
 * objects in the tree.
 *
 * Note that an object of this class should not be re-used across multiple
 * context trees. (Although with the current implementation there is no
 * practical objection.)
 */
class proxy_async_tree_context_base
{
 public:
    proxy_async_tree_context_base(
        inner_resources& resources, std::string proxy_name);

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
        return resources_.get_proxy(proxy_name_);
    }

    spdlog::logger&
    get_logger() const noexcept
    {
        return *logger_;
    }

 private:
    inner_resources& resources_;
    std::string proxy_name_;
    std::shared_ptr<spdlog::logger> logger_;
};

/*
 * Context that can be used to asynchronously resolve requests on a remote
 * machine. It acts as a proxy for a local_async_context_base object on a
 * remote server.
 *
 * This class needs thread synchronization between the main thread (the
 * resolve_request() call) and zero or more querier threads.
 * - The main thread calls set_remote_id() or fail_remote_id(). Queriers may
 *   need to wait for a valid remote_id.
 *   * set_remote_id(): queriers call remote_id_future_.get() and set
 *     remote_id_ to the returned value. remote_id_ is atomic.
 *   * fail_remote_id(): queriers call remote_id_future_.get().
 *     fail_remote_id() sets an exception on the related promise, causing
 *     that exception to propagate to each querier.
 * - One or more queriers call get_num_subs(), which populates have_subs_ and
 *   subs_ if that wasn't done before. Synchronization through subs_mutex_.
 */
class proxy_async_context_base : public remote_async_context_intf
{
 public:
    proxy_async_context_base(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx);

    // Once created, these objects should not be moved.
    proxy_async_context_base(proxy_async_context_base const&) = delete;
    void
    operator=(proxy_async_context_base const&)
        = delete;
    proxy_async_context_base(proxy_async_context_base&&) = delete;
    void
    operator=(proxy_async_context_base&&)
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

    remote_proxy&
    get_proxy() const override
    {
        return tree_ctx_->get_proxy();
    }

    virtual std::string const&
    domain_name() const override
        = 0;

    virtual service_config
    make_config() const override
        = 0;

    // async_context_intf
    async_id
    get_id() const override
    {
        return id_;
    }

    std::size_t
    get_num_subs() const override;

    async_context_intf&
    get_sub(std::size_t ix) override;

    cppcoro::task<async_status>
    get_status_coro() override;

    cppcoro::task<void>
    request_cancellation_coro() override;

    // remote_async_context_intf
    async_id
    get_remote_id() override
    {
        return remote_id_;
    }

 protected:
    std::shared_ptr<proxy_async_tree_context_base> tree_ctx_;
    async_id id_;
    std::atomic<async_id> remote_id_{NO_ASYNC_ID};

    // This mutex regulates write access to have_subs_ and subs_.
    std::mutex subs_mutex_;
    bool have_subs_{false};
    // Using unique_ptr because class proxy_async_context_base has no default
    // constructor and no copy constructor.
    std::vector<std::unique_ptr<proxy_async_context_base>> subs_;

    virtual std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx, bool is_req)
        = 0;

    void
    ensure_subs() const;

    void
    ensure_subs_no_const();

    virtual void
    wait_on_remote_id()
        = 0;
};

/*
 * Context that can be used to asynchronously resolve root requests on a remote
 * machine.
 */
class root_proxy_async_context_base : public proxy_async_context_base
{
 public:
    root_proxy_async_context_base(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx);

    virtual ~root_proxy_async_context_base();

    // remote_context_intf
    std::string const&
    domain_name() const override
        = 0;

    // async_context_intf
    bool
    is_req() const override
    {
        return true;
    }

    // remote_async_context_intf
    void
    set_remote_id(async_id remote_id) override;

    void
    fail_remote_id() noexcept override;

 private:
    std::promise<async_id> remote_id_promise_;
    // Using shared_future to allow get() from multiple threads
    std::shared_future<async_id> remote_id_future_;

    void
    wait_on_remote_id() override;

    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx,
        bool is_req) override
        = 0;
};

/*
 * Context that can be used to asynchronously resolve non-root requests on a
 * remote machine.
 */
class non_root_proxy_async_context_base : public proxy_async_context_base
{
 public:
    non_root_proxy_async_context_base(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx, bool is_req);

    virtual ~non_root_proxy_async_context_base();

    // remote_context_intf
    std::string const&
    domain_name() const override
        = 0;

    // async_context_intf
    bool
    is_req() const override
    {
        return is_req_;
    }

    // remote_async_context_intf
    void
    set_remote_id(async_id remote_id) override;

    void
    fail_remote_id() noexcept override;

 private:
    bool is_req_;

    void
    wait_on_remote_id() override;

    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(
        std::shared_ptr<proxy_async_tree_context_base> tree_ctx,
        bool is_req) override
        = 0;
};

// Adds a newly created local_async_context_base object to the async db.
void
register_local_async_ctx(std::shared_ptr<local_async_context_base> ctx);

} // namespace cradle

#endif
