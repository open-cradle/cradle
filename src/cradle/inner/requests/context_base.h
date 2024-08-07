#ifndef CRADLE_INNER_REQUEST_CONTEXT_BASE_H
#define CRADLE_INNER_REQUEST_CONTEXT_BASE_H

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/test_context.h>
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

class data_owner_factory
{
 public:
    data_owner_factory(inner_resources& resources);

    // Cf. local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory);

    void
    track_blob_file_writers();

    void
    on_value_complete();

 private:
    inner_resources& resources_;
    bool tracking_blob_file_writers_{false};
    // The blob_file_writer objects allocated during the resolution of requests
    // associated with this factory; only if track_blob_file_writers() was
    // called.
    std::vector<std::shared_ptr<blob_file_writer>> blob_file_writers_;
};

/*
 * An abstract base class that can be used to synchronously resolve requests.
 * It offers all context features other than the asynchronous functionality
 * (i.e., implements all context interfaces other than async_context_intf).
 */
class sync_context_base : public virtual local_sync_context_intf,
                          public remote_context_intf,
                          public virtual caching_context_intf,
                          public introspective_context_intf
{
 public:
    sync_context_base(
        inner_resources& resources,
        tasklet_tracker* tasklet,
        std::string proxy_name);

    // context_intf
    inner_resources&
    get_resources() override
    {
        return resources_;
    }

    bool
    remotely() const override
    {
        // TODO wrap proxy_name in a class
        return !proxy_name_.empty();
    }

    bool
    is_async() const override
    {
        return false;
    }

    virtual std::string const&
    domain_name() const override
        = 0;

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override;

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override;

    void
    track_blob_file_writers() override;

    void
    on_value_complete() override;

    // remote_context_intf
    remote_proxy&
    get_proxy() const override;

    virtual service_config
    make_config(bool need_record_lock) const override
        = 0;

    // introspective_context_intf
    tasklet_tracker*
    get_tasklet() override;

    void
    push_tasklet(tasklet_tracker& tasklet) override;

    void
    pop_tasklet() override;

 protected:
    inner_resources& resources_;
    std::string proxy_name_;
    std::vector<tasklet_tracker*> tasklets_;
    data_owner_factory the_data_owner_factory_;
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

    // Cf. local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory);

    void
    track_blob_file_writers();

    void
    on_value_complete();

 private:
    inner_resources& resources_;
    cppcoro::cancellation_source csource_;
    cppcoro::cancellation_token ctoken_;
    std::shared_ptr<spdlog::logger> logger_;
    data_owner_factory the_data_owner_factory_;
};

/*
 * Context that can be used to asynchronously resolve requests, on the local
 * machine.
 *
 * Relates to a single request, or a non-request argument of such a request,
 * which will be resolved on the local machine.
 */
class local_async_context_base : public virtual local_async_context_intf,
                                 public virtual caching_context_intf,
                                 public introspective_context_intf
{
 public:
    local_async_context_base(
        local_tree_context_base& tree_ctx,
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
    inner_resources&
    get_resources() override
    {
        return tree_ctx_.get_resources();
    }

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

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override;

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override;

    void
    track_blob_file_writers() override;

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
    void
    set_essentials(std::unique_ptr<request_essentials> essentials) override;

    request_essentials
    get_essentials() const override;

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

    cppcoro::task<void>
    reschedule_if_opportune() override;

    async_status
    get_status() override
    {
        return status_;
    }

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
    request_cancellation() override
    {
        tree_ctx_.request_cancellation();
    }

    bool
    is_cancellation_requested() const noexcept override;

    void
    throw_async_cancelled() const override;

    void
    set_delegate(std::shared_ptr<async_context_intf> delegate) override;

    std::shared_ptr<async_context_intf>
    get_delegate() override;

    // introspective_context_intf
    tasklet_tracker*
    get_tasklet() override;

    void
    push_tasklet(tasklet_tracker& tasklet) override;

    void
    pop_tasklet() override;

    // Other
    void
    add_sub(std::size_t ix, std::shared_ptr<local_async_context_base> sub);

    local_tree_context_base&
    get_tree_context()
    {
        return tree_ctx_;
    }

 private:
    local_tree_context_base& tree_ctx_;
    local_async_context_base* parent_;
    bool is_req_;
    std::unique_ptr<request_essentials> essentials_;
    async_id const id_;
    std::atomic<async_status> status_;
    std::string errmsg_;
    // Using shared_ptr ensures that local_async_context_base objects are not
    // relocated during tree build-up / visit.
    // It cannot be unique_ptr because there can be two owners: the parent
    // context, and the async_db.
    std::vector<std::shared_ptr<local_async_context_base>> subs_;
    std::atomic<int> num_subs_not_running_;
    std::vector<tasklet_tracker*> tasklets_;

    spdlog::logger&
    get_logger() noexcept
    {
        return tree_ctx_.get_logger();
    }

    std::weak_ptr<async_context_intf> delegate_;
    std::unique_ptr<cppcoro::cancellation_registration> delegate_cancellation_;

    bool
    decide_reschedule_sub();

    void
    cancel_delegate() noexcept;
};

class root_local_async_context_base : public local_async_context_base,
                                      public root_local_async_context_intf,
                                      public test_context_intf
{
 public:
    root_local_async_context_base(local_tree_context_base& tree_ctx);

    // local_async_context_intf
    void
    update_status(async_status status) override;

    // root_local_async_context_intf
    virtual std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override = 0;

    void
    using_result() override;

    void
    set_result(blob result) override;

    blob
    get_result() override;

    void
    set_cache_record_id(remote_cache_record_id record_id) override
    {
        cache_record_id_.store(record_id);
    }

    remote_cache_record_id
    get_cache_record_id() const override
    {
        return cache_record_id_.load();
    }

    // test_context_intf
    virtual void
    apply_fail_submit_async() override
    {
    }

    virtual void
    apply_resolve_async_delay() override
    {
    }

 private:
    bool using_result_{false};
    blob result_;
    std::atomic<remote_cache_record_id> cache_record_id_;

    void
    check_set_get_result_precondition(bool is_get_result);
};

class non_root_local_async_context_base : public local_async_context_base
{
 public:
    non_root_local_async_context_base(
        local_tree_context_base& tree_ctx,
        local_async_context_base* parent,
        bool is_req,
        std::unique_ptr<request_essentials> essentials);
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
    visit_req_arg(
        std::size_t ix,
        std::unique_ptr<request_essentials> essentials) override;

 protected:
    local_async_context_base& ctx_;

    virtual std::unique_ptr<local_context_tree_builder_base>
    make_sub_builder(local_async_context_base& sub_ctx) = 0;

    std::shared_ptr<local_async_context_base>
    make_sub_ctx(
        std::size_t ix,
        bool is_req,
        std::unique_ptr<request_essentials> essentials);

    virtual std::shared_ptr<local_async_context_base>
    make_sub_ctx(
        local_tree_context_base& tree_ctx,
        std::size_t ix,
        bool is_req,
        std::unique_ptr<request_essentials> essentials)
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

    remote_proxy&
    get_proxy() const
    {
        return resources_.get_proxy(proxy_name_);
    }

    // Cancels associated proxy_async_context_base::schedule_after() calls
    void
    request_local_cancellation()
    {
        csource_.request_cancellation();
    }

    cppcoro::cancellation_token
    get_cancellation_token()
    {
        return ctoken_;
    }

    spdlog::logger&
    get_logger() const noexcept
    {
        return *logger_;
    }

 private:
    inner_resources& resources_;
    std::string proxy_name_;
    cppcoro::cancellation_source csource_;
    cppcoro::cancellation_token ctoken_;
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
    // tree_ctx will be owned by a derived object, so may not exist in
    // ~proxy_async_context_base().
    proxy_async_context_base(proxy_async_tree_context_base& tree_ctx);

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
    inner_resources&
    get_resources() override
    {
        return tree_ctx_.get_resources();
    }

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

    virtual std::string const&
    domain_name() const override
        = 0;

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override;

    // remote_context_intf
    remote_proxy&
    get_proxy() const override
    {
        return tree_ctx_.get_proxy();
    }

    virtual service_config
    make_config(bool need_record_lock) const override
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
    proxy_async_tree_context_base& tree_ctx_;
    async_id id_;
    std::atomic<async_id> remote_id_{NO_ASYNC_ID};

    // This mutex regulates write access to have_subs_ and subs_.
    std::mutex subs_mutex_;
    bool have_subs_{false};
    // Using unique_ptr because class proxy_async_context_base has no default
    // constructor and no copy constructor.
    std::vector<std::unique_ptr<proxy_async_context_base>> subs_;

    virtual std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(proxy_async_tree_context_base& tree_ctx, bool is_req) = 0;

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
    root_proxy_async_context_base(proxy_async_tree_context_base& tree_ctx);

    // context_intf
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

    void
    make_introspective() override;

    bool
    introspective() const override
    {
        return introspective_;
    }

 protected:
    // To be called from a derived object's destructor. The functionality
    // cannot be in ~root_proxy_async_context_base() as the tree_ctx_ object
    // may no longer exist at that time.
    void
    finish_remote() noexcept;

 private:
    std::promise<async_id> remote_id_promise_;
    // Using shared_future to allow get() from multiple threads
    std::shared_future<async_id> remote_id_future_;
    bool introspective_{false};

    void
    wait_on_remote_id() override;

    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(proxy_async_tree_context_base& tree_ctx, bool is_req) override
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
        proxy_async_tree_context_base& tree_ctx, bool is_req);

    // context_intf
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

    void
    make_introspective() override;

    bool
    introspective() const override;

 private:
    bool is_req_;

    void
    wait_on_remote_id() override;

    // proxy_async_context_base
    std::unique_ptr<proxy_async_context_base>
    make_sub_ctx(proxy_async_tree_context_base& tree_ctx, bool is_req) override
        = 0;
};

// Adds a newly created local_async_context_base object to the async db.
void
register_local_async_ctx(std::shared_ptr<local_async_context_base> ctx);

} // namespace cradle

#endif
