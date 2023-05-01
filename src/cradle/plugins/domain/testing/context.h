#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H

#include <atomic>
#include <memory>
#include <vector>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

struct testing_request_context final
    : public cached_introspective_context_intf,
      public sync_context_intf,
      public remote_context_intf
{
    inner_resources& service;

    testing_request_context(
        inner_resources& service,
        tasklet_tracker* tasklet,
        bool remotely = false);

    inner_resources&
    get_resources() override;

    immutable_cache&
    get_cache() override;

    tasklet_tracker*
    get_tasklet() override;

    void
    push_tasklet(tasklet_tracker* tasklet) override;

    void
    pop_tasklet() override;

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

    void
    proxy_name(std::string const& name);

    std::string const&
    proxy_name() const override;

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<remote_context_intf>
    local_clone() const override;

    std::shared_ptr<blob_file_writer>
    make_blob_file_writer(size_t size);

 private:
    std::vector<tasklet_tracker*> tasklets_;
    bool remotely_;
    std::string proxy_name_;
    std::string domain_name_{"testing"};
};

static_assert(ValidContext<testing_request_context>);

// Tree-level context, shared by all request-level contexts in a tree.
// Used for local contexts only.
class local_atst_tree_context
{
 public:
    local_atst_tree_context(inner_resources& inner)
        : inner_{inner}, ctoken_{csource_.token()}
    {
    }

    inner_resources&
    get_inner_resources()
    {
        return inner_;
    }

    immutable_cache&
    get_cache()
    {
        return inner_.memory_cache();
    }

    async_db*
    get_async_db()
    {
        return inner_.get_async_db();
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

 private:
    inner_resources& inner_;
    cppcoro::cancellation_source csource_;
    cppcoro::cancellation_token ctoken_;
};

// Context, relating to a single task (= request-to-be-resolved),
// or a non-request argument of such a request;
// on the local machine.
class local_atst_context final : public cached_introspective_context_intf,
                                 public local_async_context_intf
{
 public:
    local_atst_context(
        std::shared_ptr<local_atst_tree_context> tree_ctx,
        local_atst_context* parent,
        bool is_req);

    local_atst_context(local_atst_context&) = delete;
    void
    operator=(local_atst_context&)
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

    // cached_context_intf
    inner_resources&
    get_resources() override
    {
        return tree_ctx_->get_inner_resources();
    }

    immutable_cache&
    get_cache() override
    {
        return tree_ctx_->get_cache();
    }

    // introspective_context_intf
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

    local_atst_context&
    get_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    cppcoro::task<async_status>
    get_status_coro() override;

    cppcoro::task<void>
    request_cancellation_coro() override;

    // local_async_context_intf
    std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() override;

    bool
    decide_reschedule() override;

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

    cppcoro::static_thread_pool&
    get_thread_pool() override
    {
        return get_resources().get_async_thread_pool();
    }

    void
    request_cancellation() override
    {
        tree_ctx_->request_cancellation();
    }

    bool
    is_cancellation_requested() const noexcept override;

    void
    throw_if_cancellation_requested() const override;

    void
    set_result(blob result) override;

    blob
    get_result() override;

    // Other
    void
    add_sub(std::size_t ix, std::shared_ptr<local_atst_context> sub);

    std::shared_ptr<local_atst_tree_context>
    get_tree_context()
    {
        return tree_ctx_;
    }

    bool
    decide_reschedule_sub();

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
    std::vector<std::shared_ptr<local_atst_context>> subs_;
    std::atomic<int> num_subs_not_running_;
};

static_assert(ValidContext<local_atst_context>);

class local_atst_context_tree_builder : public req_visitor_intf
{
 public:
    local_atst_context_tree_builder(local_atst_context& actx);

    void
    visit_val_arg(std::size_t ix) override;

    std::unique_ptr<req_visitor_intf>
    visit_req_arg(std::size_t ix) override;

 private:
    local_atst_context& actx_;
};

// Creates a tree of local_atst_context objects, reflecting the structure of
// the request tree of which `req' is the root request.
// TODO Req must have visit()
template<typename Req>
std::shared_ptr<local_atst_context>
make_local_async_ctx_tree(
    std::shared_ptr<local_atst_tree_context> tree_ctx, Req const& req)
{
    auto root_ctx{
        std::make_shared<local_atst_context>(tree_ctx, nullptr, true)};
    if (auto* db = tree_ctx->get_async_db())
    {
        db->add(root_ctx);
    }
    local_atst_context_tree_builder builder{*root_ctx};
    req.visit(builder);
    return root_ctx;
}

// Tree-level context, shared by all request-level contexts in a tree.
// Used for proxy contexts only.
class proxy_atst_tree_context
{
 public:
    proxy_atst_tree_context(
        inner_resources& inner, std::string const& proxy_name);

    inner_resources&
    get_inner_resources()
    {
        return inner_;
    }

    std::string const&
    get_proxy_name() const
    {
        return proxy_name_;
    }

    remote_proxy&
    get_proxy() const;

 private:
    inner_resources& inner_;
    std::string proxy_name_;
    mutable std::unique_ptr<remote_proxy*> proxy_;
};

// Proxy for a local_atst_context object on an RPC server
class proxy_atst_context final : public remote_async_context_intf
{
 public:
    proxy_atst_context(
        std::shared_ptr<proxy_atst_tree_context> tree_ctx,
        bool is_root,
        bool is_req);

    ~proxy_atst_context();

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
    proxy_name() const override;

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<remote_context_intf>
    local_clone() const override;

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

    // remote_async_context_intf
    std::shared_ptr<local_async_context_intf>
    local_async_clone() const override;

    remote_async_context_intf&
    add_sub(async_id sub_remote_id, bool is_req) override;

    proxy_atst_context&
    get_sub(std::size_t ix) override
    {
        return *subs_[ix];
    }

    void
    set_remote_id(async_id remote_id) override
    {
        remote_id_ = remote_id;
    }

    async_id
    get_remote_id() override;

    cppcoro::task<async_status>
    get_status_coro() override;

    cppcoro::task<void>
    request_cancellation_coro() override;

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
    // Using unique_ptr ensures that atst_context_base objects are not
    // relocated during tree build-up / visit.
    std::vector<std::unique_ptr<proxy_atst_context>> subs_;

    remote_proxy&
    get_proxy();
};

static_assert(ValidContext<proxy_atst_context>);

// Creates a proxy_atst_context object for the root node in a context tree.
// Subobjects to be added later.
inline proxy_atst_context
make_remote_async_ctx(std::shared_ptr<proxy_atst_tree_context> tree_ctx)
{
    return proxy_atst_context{std::move(tree_ctx), true, true};
}

} // namespace cradle

#endif
