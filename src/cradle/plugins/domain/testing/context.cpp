#include <atomic>
#include <cassert>
#include <stdexcept>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

// TODO make everything thread-safe

namespace cradle {

testing_request_context::testing_request_context(
    inner_resources& resources,
    tasklet_tracker* tasklet,
    bool remotely,
    std::string proxy_name)
    : resources_{resources},
      remotely_{remotely},
      proxy_name_{std::move(proxy_name)}
{
    if (tasklet)
    {
        tasklets_.push_back(tasklet);
    }
}

std::shared_ptr<local_context_intf>
testing_request_context::local_clone() const
{
    // TODO tasklets should continue in the clone
    return std::make_shared<testing_request_context>(
        resources_, nullptr, false, "");
}

tasklet_tracker*
testing_request_context::get_tasklet()
{
    if (tasklets_.empty())
    {
        return nullptr;
    }
    return tasklets_.back();
}

void
testing_request_context::push_tasklet(tasklet_tracker* tasklet)
{
    tasklets_.push_back(tasklet);
}

void
testing_request_context::pop_tasklet()
{
    tasklets_.pop_back();
}

namespace {

async_id
allocate_async_id()
{
    static std::atomic<async_id> next_id{1};
    return next_id.fetch_add(1);
}

async_db*
get_async_db(local_atst_tree_context& tree_ctx)
{
    return tree_ctx.get_resources().get_async_db();
}

} // namespace

local_atst_tree_context::local_atst_tree_context(inner_resources& resources)
    : resources_{resources},
      ctoken_{csource_.token()},
      logger_{spdlog::get("cradle")}
{
}

local_atst_context::local_atst_context(
    std::shared_ptr<local_atst_tree_context> tree_ctx,
    local_atst_context* parent,
    bool is_req)
    : tree_ctx_{std::move(tree_ctx)},
      parent_{parent},
      is_req_{is_req},
      id_{allocate_async_id()},
      status_{is_req ? async_status::CREATED : async_status::FINISHED},
      num_subs_not_running_{0}
{
    auto& logger{tree_ctx_->get_logger()};
    auto parent_id = parent ? parent->get_id() : 0;
    logger.info(
        "local_atst_context {} (parent {}, {}): created, status {}",
        id_,
        parent_id,
        is_req ? "REQ" : "VAL",
        status_);
}

void
local_atst_context::add_sub(
    std::size_t ix, std::shared_ptr<local_atst_context> sub)
{
    assert(ix == subs_.size());
    if (sub->is_req())
    {
        num_subs_not_running_ += 1;
    }
    subs_.push_back(std::move(sub));
}

cppcoro::task<async_status>
local_atst_context::get_status_coro()
{
    co_return get_status();
}

cppcoro::task<void>
local_atst_context::request_cancellation_coro()
{
    request_cancellation();
    co_return;
}

std::unique_ptr<req_visitor_intf>
local_atst_context::make_ctx_tree_builder()
{
    return std::make_unique<local_atst_context_tree_builder>(*this);
}

cppcoro::task<void>
local_atst_context::reschedule_if_opportune()
{
    auto& logger{tree_ctx_->get_logger()};
    if (!is_req_)
    {
        // Violating this function's precondition
        logger.error(
            "local_atst_context {} reschedule_if_opportune(): not a request",
            id_);
        assert(false);
    }
    else if (!parent_)
    {
        // The root request is already running on a dedicated thread
        logger.debug(
            "local_atst_context {} reschedule_if_opportune(): false due to "
            "root request",
            id_);
    }
    else
    {
        // Let the parent decide: its last subrequest to start running can
        // continue on the parent's thread, the other ones should reschedule
        bool reschedule = parent_->decide_reschedule_sub();
        logger.debug(
            "local_atst_context {} reschedule_if_opportune(): {} due to "
            "parent",
            id_,
            reschedule);
        if (reschedule)
        {
            auto& thread_pool{get_resources().get_async_thread_pool()};
            co_await thread_pool.schedule();
        }
    }
    co_return;
}

bool
local_atst_context::decide_reschedule_sub()
{
    // Return true if there will be at least one more sub to start running,
    // after the current one
    return num_subs_not_running_.fetch_sub(1) > 1;
}

async_status
local_atst_context::get_status()
{
    auto status = status_;
#if 0
    // TODO do we need this?
    if (status < async_status::CANCELLING
        && get_cancellation_token().is_cancellation_requested())
    {
        auto& logger{tree_ctx_->get_logger()};
        logger.info(
            "local_atst_context {}: get_status() detected cancellation",
            id_);
        status = async_status::CANCELLING;
        update_status(status);
    }
#endif
    return status;
}

void
local_atst_context::update_status(async_status status)
{
    auto& logger{tree_ctx_->get_logger()};
    logger.info(
        "local_atst_context {} update_status {} -> {}", id_, status_, status);
    // Invariant: if this status is FINISHED, then all subs too
    // (Subs won't be finished yet if the result came from a cache.)
    if (status_ != async_status::FINISHED && status == async_status::FINISHED)
    {
        for (auto sub : subs_)
        {
            sub->update_status(status);
        }
    }
    status_ = status;
}

void
local_atst_context::update_status_error(std::string const& errmsg)
{
    auto& logger{tree_ctx_->get_logger()};
    logger.info(
        "local_atst_context {} update_status_error: {} -> ERROR: {}",
        id_,
        status_,
        errmsg);
    status_ = async_status::ERROR;
    errmsg_ = errmsg;
}

void
local_atst_context::set_result(blob result)
{
    // TODO update status to FINISHED here?
    result_ = std::move(result);
}

blob
local_atst_context::get_result()
{
    if (status_ != async_status::FINISHED)
    {
        throw std::logic_error(fmt::format(
            "local_atst_context {} get_result() called for status {}",
            id_,
            status_));
    }
    // TODO maybe store shared_ptr<blob>
    return result_;
}

bool
local_atst_context::is_cancellation_requested() const noexcept
{
    auto token{tree_ctx_->get_cancellation_token()};
    return token.is_cancellation_requested();
}

void
local_atst_context::throw_if_cancellation_requested() const
{
    if (is_cancellation_requested())
    {
        throw async_cancelled{
            fmt::format("local_atst_context {} cancelled", id_)};
    }
}

std::shared_ptr<local_atst_context>
make_root_local_atst_context(std::shared_ptr<local_atst_tree_context> tree_ctx)
{
    auto root_ctx{
        std::make_shared<local_atst_context>(tree_ctx, nullptr, true)};
    if (auto* db = get_async_db(*tree_ctx))
    {
        db->add(root_ctx);
    }
    return root_ctx;
}

// TODO can this be generalized for other context types?
local_atst_context_tree_builder::local_atst_context_tree_builder(
    local_atst_context& ctx)
    : ctx_{ctx}
{
}

void
local_atst_context_tree_builder::visit_val_arg(std::size_t ix)
{
    make_sub_ctx(ix, false);
}

std::unique_ptr<req_visitor_intf>
local_atst_context_tree_builder::visit_req_arg(std::size_t ix)
{
    auto sub_ctx{make_sub_ctx(ix, true)};
    return std::make_unique<local_atst_context_tree_builder>(*sub_ctx);
}

std::shared_ptr<local_atst_context>
local_atst_context_tree_builder::make_sub_ctx(std::size_t ix, bool is_req)
{
    auto tree_ctx{ctx_.get_tree_context()};
    auto sub_ctx
        = std::make_shared<local_atst_context>(tree_ctx, &ctx_, is_req);
    ctx_.add_sub(ix, sub_ctx);
    if (auto* db = get_async_db(*tree_ctx))
    {
        db->add(sub_ctx);
    }
    return sub_ctx;
}

proxy_atst_tree_context::proxy_atst_tree_context(
    inner_resources& resources, std::string proxy_name)
    : resources_(resources),
      proxy_name_{std::move(proxy_name)},
      proxy_{find_proxy(proxy_name_)},
      logger_{spdlog::get("cradle")}
{
}

proxy_atst_context::proxy_atst_context(
    std::shared_ptr<proxy_atst_tree_context> tree_ctx, bool is_req)
    : tree_ctx_{std::move(tree_ctx)}, is_req_{is_req}, id_{allocate_async_id()}
{
}

cppcoro::task<std::size_t>
proxy_atst_context::get_num_subs() const
{
    co_await ensure_subs();
    co_return subs_.size();
}

async_context_intf&
proxy_atst_context::get_sub(std::size_t ix)
{
    assert(have_subs_);
    return *subs_[ix];
}

cppcoro::task<async_status>
proxy_atst_context::get_status_coro()
{
    wait_on_remote_id();
    auto& proxy{get_proxy()};
    co_return proxy.get_async_status(remote_id_);
}

cppcoro::task<void>
proxy_atst_context::request_cancellation_coro()
{
    wait_on_remote_id();
    auto& proxy{get_proxy()};
    proxy.request_cancellation(remote_id_);
    co_return;
}

proxy_atst_context&
proxy_atst_context::get_remote_sub(std::size_t ix)
{
    return *subs_[ix];
}

std::shared_ptr<local_atst_context>
proxy_atst_context::make_local_clone() const
{
    return std::make_shared<local_atst_context>(
        std::make_shared<local_atst_tree_context>(tree_ctx_->get_resources()),
        nullptr,
        is_req_);
}

cppcoro::task<>
proxy_atst_context::ensure_subs() const
{
    return const_cast<proxy_atst_context*>(this)->ensure_subs_no_const();
}

cppcoro::task<>
proxy_atst_context::ensure_subs_no_const()
{
    if (have_subs_)
    {
        co_return;
    }
    wait_on_remote_id();
    // TODO wait until get_sub_contexts() precondition holds:
    // status for remote_id is SUBS_RUNNING, SELF_RUNNING or FINISHED
    auto& proxy{get_proxy()};
    auto specs = proxy.get_sub_contexts(remote_id_);
    for (auto& spec : specs)
    {
        auto [sub_aid, is_req] = spec;
        auto sub_ctx
            = std::make_unique<non_root_proxy_atst_context>(tree_ctx_, is_req);
        sub_ctx->set_remote_id(sub_aid);
        subs_.push_back(std::move(sub_ctx));
    }
    have_subs_ = true;
}

root_proxy_atst_context::root_proxy_atst_context(
    std::shared_ptr<proxy_atst_tree_context> tree_ctx, bool is_req)
    : proxy_atst_context{tree_ctx, is_req},
      remote_id_future_{remote_id_promise_.get_future()}
{
}

root_proxy_atst_context::~root_proxy_atst_context()
{
    // Clean up the context tree on the server once per proxy context tree.
    // There must have been a set_remote_id() or fail_remote_id() call for this
    // root context.
    if (remote_id_ != NO_ASYNC_ID)
    {
        // Destructor must not throw
        try
        {
            auto& proxy{get_proxy()};
            proxy.finish_async(remote_id_);
        }
        catch (std::exception& e)
        {
            auto& logger{tree_ctx_->get_logger()};
            try
            {
                logger.error("~root_proxy_atst_context() caught {}", e.what());
            }
            catch (...)
            {
            }
        }
    }
}

void
root_proxy_atst_context::set_remote_id(async_id remote_id)
{
    // This could throw std::future_error, but that should be possible only
    // when the caller is misbehaving.
    remote_id_promise_.set_value(remote_id);
}

void
root_proxy_atst_context::fail_remote_id() noexcept
{
    try
    {
        remote_id_promise_.set_exception(std::current_exception());
    }
    catch (std::future_error const& e)
    {
        try
        {
            // Everything must be noexcept here
            auto& logger(tree_ctx_->get_logger());
            logger.warn(
                "root_proxy_atst_context::fail_remote_id caught {}", e.what());
        }
        catch (...)
        {
        }
    }
}

// This may cause the current thread to block.
// Cannot use cppcoro::async_manual_reset_event because it could be set from
// an exception handler or destructor which must not throw, and the unblocked
// call will continue on that thread.
void
root_proxy_atst_context::wait_on_remote_id()
{
    if (remote_id_ == NO_ASYNC_ID)
    {
        remote_id_ = remote_id_future_.get();
    }
}

non_root_proxy_atst_context::non_root_proxy_atst_context(
    std::shared_ptr<proxy_atst_tree_context> tree_ctx, bool is_req)
    : proxy_atst_context{tree_ctx, is_req}
{
}

void
non_root_proxy_atst_context::set_remote_id(async_id remote_id)
{
    remote_id_ = remote_id;
}

void
non_root_proxy_atst_context::fail_remote_id() noexcept
{
    assert(false);
}

void
non_root_proxy_atst_context::wait_on_remote_id()
{
    // For non-root contexts, remote_id_ is set on object creation
}

} // namespace cradle
