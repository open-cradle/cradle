#include <atomic>
#include <cassert>
#include <stdexcept>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/wait_async.h>
#include <cradle/inner/requests/context_base.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// The mutex should be part of data_owner_factory, but that would make
// thinknode_request_context non-copyable.
static std::mutex data_owner_factory_mutex;

data_owner_factory::data_owner_factory(inner_resources& resources)
    : resources_{resources}
{
}

cppcoro::task<>
sync_context_base::schedule_after(std::chrono::milliseconds delay)
{
    auto& io_svc{resources_.the_io_service()};
    // Note not cancellable
    co_await io_svc.schedule_after(delay);
}

std::shared_ptr<data_owner>
data_owner_factory::make_data_owner(std::size_t size, bool use_shared_memory)
{
    std::shared_ptr<data_owner> owner;
    if (use_shared_memory)
    {
        std::scoped_lock lock(data_owner_factory_mutex);
        auto writer = resources_.make_blob_file_writer(size);
        if (tracking_blob_file_writers_)
        {
            blob_file_writers_.push_back(writer);
        }
        owner = writer;
    }
    else
    {
        owner = make_shared_buffer(size);
    }
    return owner;
}

void
data_owner_factory::track_blob_file_writers()
{
    std::scoped_lock lock(data_owner_factory_mutex);
    tracking_blob_file_writers_ = true;
}

void
data_owner_factory::on_value_complete()
{
    std::scoped_lock lock(data_owner_factory_mutex);
    if (!tracking_blob_file_writers_)
    {
        throw std::logic_error(
            "on_value_complete() without preceding track_blob_file_writers()");
    }
    for (auto writer : blob_file_writers_)
    {
        writer->on_write_completed();
    }
    blob_file_writers_.clear();
}

sync_context_base::sync_context_base(
    inner_resources& resources,
    tasklet_tracker* tasklet,
    std::string proxy_name)
    : resources_{resources},
      proxy_name_{std::move(proxy_name)},
      the_data_owner_factory_{resources}
{
    if (tasklet)
    {
        tasklets_.push_back(tasklet);
    }
}

std::shared_ptr<data_owner>
sync_context_base::make_data_owner(std::size_t size, bool use_shared_memory)
{
    return the_data_owner_factory_.make_data_owner(size, use_shared_memory);
}

void
sync_context_base::track_blob_file_writers()
{
    the_data_owner_factory_.track_blob_file_writers();
}

void
sync_context_base::on_value_complete()
{
    the_data_owner_factory_.on_value_complete();
}

remote_proxy&
sync_context_base::get_proxy() const
{
    return resources_.get_proxy(proxy_name_);
}

tasklet_tracker*
sync_context_base::get_tasklet()
{
    if (tasklets_.empty())
    {
        return nullptr;
    }
    return tasklets_.back();
}

void
sync_context_base::push_tasklet(tasklet_tracker& tasklet)
{
    tasklets_.push_back(&tasklet);
}

void
sync_context_base::pop_tasklet()
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
get_async_db(local_tree_context_base& tree_ctx)
{
    return tree_ctx.get_resources().get_async_db();
}

// Matches if the subs on the remote are available for retrieval, i.e., if the
// get_sub_contexts() precondition holds.
class subs_available_matcher : public async_status_matcher
{
 public:
    subs_available_matcher(spdlog::logger& logger);

    bool operator()(async_status) const override;
};

subs_available_matcher::subs_available_matcher(spdlog::logger& logger)
    : async_status_matcher{"subs_available_matcher", logger}
{
}

bool
subs_available_matcher::operator()(async_status status) const
{
    bool done = status >= async_status::SUBS_RUNNING;
    report_status(status, done);
    return done;
}

} // namespace

local_tree_context_base::local_tree_context_base(inner_resources& resources)
    : resources_{resources},
      ctoken_{csource_.token()},
      logger_{spdlog::get("cradle")},
      the_data_owner_factory_{resources}
{
}

std::shared_ptr<data_owner>
local_tree_context_base::make_data_owner(
    std::size_t size, bool use_shared_memory)
{
    return the_data_owner_factory_.make_data_owner(size, use_shared_memory);
}

void
local_tree_context_base::track_blob_file_writers()
{
    the_data_owner_factory_.track_blob_file_writers();
}

void
local_tree_context_base::on_value_complete()
{
    the_data_owner_factory_.on_value_complete();
}

local_async_context_base::local_async_context_base(
    local_tree_context_base& tree_ctx,
    local_async_context_base* parent,
    bool is_req)
    : tree_ctx_{tree_ctx},
      parent_{parent},
      is_req_{is_req},
      id_{allocate_async_id()},
      status_{is_req ? async_status::CREATED : async_status::FINISHED},
      num_subs_not_running_{0}
{
    auto& logger{tree_ctx_.get_logger()};
    auto parent_id = parent ? parent->get_id() : 0;
    logger.info(
        "local_async_context_base {} (parent {}, {}): created, status {}",
        id_,
        parent_id,
        is_req ? "REQ" : "VAL",
        status_.load(std::memory_order_relaxed));
}

cppcoro::task<>
local_async_context_base::schedule_after(std::chrono::milliseconds delay)
{
    auto& io_svc{get_resources().the_io_service()};
    co_await io_svc.schedule_after(delay, tree_ctx_.get_cancellation_token());
}

std::shared_ptr<data_owner>
local_async_context_base::make_data_owner(
    std::size_t size, bool use_shared_memory)
{
    return tree_ctx_.make_data_owner(size, use_shared_memory);
}

void
local_async_context_base::track_blob_file_writers()
{
    tree_ctx_.track_blob_file_writers();
}

void
local_async_context_base::on_value_complete()
{
    tree_ctx_.on_value_complete();
}

// Returns the last tasklet from the vector formed by concatenating the tasklet
// vectors from all ancestor contexts, and from this context.
tasklet_tracker*
local_async_context_base::get_tasklet()
{
    if (!tasklets_.empty())
    {
        return tasklets_.back();
    }
    else if (parent_ != nullptr)
    {
        return parent_->get_tasklet();
    }
    else
    {
        return nullptr;
    }
}

void
local_async_context_base::push_tasklet(tasklet_tracker& tasklet)
{
    tasklets_.push_back(&tasklet);
}

void
local_async_context_base::pop_tasklet()
{
    tasklets_.pop_back();
}

void
local_async_context_base::add_sub(
    std::size_t ix, std::shared_ptr<local_async_context_base> sub)
{
    assert(ix == subs_.size());
    if (sub->is_req())
    {
        num_subs_not_running_ += 1;
    }
    subs_.push_back(std::move(sub));
}

cppcoro::task<async_status>
local_async_context_base::get_status_coro()
{
    co_return get_status();
}

cppcoro::task<void>
local_async_context_base::request_cancellation_coro()
{
    request_cancellation();
    co_return;
}

cppcoro::task<void>
local_async_context_base::reschedule_if_opportune()
{
    auto& logger{tree_ctx_.get_logger()};
    if (!is_req_)
    {
        // Violating this function's precondition
        logger.error(
            "local_async_context_base {} reschedule_if_opportune(): not a "
            "request",
            id_);
        assert(false);
    }
    else if (!parent_)
    {
        // The root request is already running on a dedicated thread
        logger.debug(
            "local_async_context_base {} reschedule_if_opportune(): false due "
            "to "
            "root request",
            id_);
    }
    else
    {
        // Let the parent decide: its last subrequest to start running can
        // continue on the parent's thread, the other ones should reschedule
        bool reschedule = parent_->decide_reschedule_sub();
        logger.debug(
            "local_async_context_base {} reschedule_if_opportune(): {} due to "
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
local_async_context_base::decide_reschedule_sub()
{
    // Return true if there will be at least one more sub to start running,
    // after the current one
    return num_subs_not_running_.fetch_sub(1) > 1;
}

void
local_async_context_base::update_status(async_status status)
{
    auto& logger{tree_ctx_.get_logger()};
    logger.info(
        "local_async_context_base {} update_status {} -> {}",
        id_,
        status_.load(std::memory_order_relaxed),
        status);
    // Invariant: if this context's status is AWAITING_RESULT or FINISHED,
    // then all its subcontexts' statuses are FINISHED.
    // (Subs won't be finished yet if the result came from a cache.)
    auto almost_finished = [](async_status status) -> bool {
        return status == async_status::AWAITING_RESULT
               || status == async_status::FINISHED;
    };
    if (!almost_finished(status_) && almost_finished(status))
    {
        for (auto sub : subs_)
        {
            sub->update_status(async_status::FINISHED);
        }
    }
    status_ = status;
}

void
local_async_context_base::update_status_error(std::string const& errmsg)
{
    auto& logger{tree_ctx_.get_logger()};
    logger.info(
        "local_async_context_base {} update_status_error: {} -> ERROR: {}",
        id_,
        status_.load(std::memory_order_relaxed),
        errmsg);
    status_ = async_status::ERROR;
    errmsg_ = errmsg;
}

bool
local_async_context_base::is_cancellation_requested() const noexcept
{
    auto token{tree_ctx_.get_cancellation_token()};
    return token.is_cancellation_requested();
}

void
local_async_context_base::throw_async_cancelled() const
{
    throw async_cancelled{
        fmt::format("local_async_context_base {} cancelled", id_)};
}

root_local_async_context_base::root_local_async_context_base(
    local_tree_context_base& tree_ctx)
    : local_async_context_base{tree_ctx, nullptr, true}
{
}

void
root_local_async_context_base::update_status(async_status status)
{
    local_async_context_base::update_status(status);
    if (using_result_ && status == async_status::FINISHED)
    {
        status = async_status::AWAITING_RESULT;
    }
    local_async_context_base::update_status(status);
}

void
root_local_async_context_base::using_result()
{
    using_result_ = true;
}

void
root_local_async_context_base::check_set_get_result_precondition(
    bool is_get_result)
{
    async_status required_status{
        is_get_result ? async_status::FINISHED
                      : async_status::AWAITING_RESULT};
    if (!using_result_ || get_status() != required_status)
    {
        throw std::logic_error(fmt::format(
            "local_async_context_base {} {}() precondition violated ({}, {})",
            get_id(),
            is_get_result ? "is_get_result" : "is_set_result",
            using_result_,
            get_status()));
    }
}

void
root_local_async_context_base::set_result(blob result)
{
    check_set_get_result_precondition(false);
    result_ = std::move(result);
    local_async_context_base::update_status(async_status::FINISHED);
}

blob
root_local_async_context_base::get_result()
{
    check_set_get_result_precondition(true);
    return result_;
}

local_context_tree_builder_base::local_context_tree_builder_base(
    local_async_context_base& ctx)
    : ctx_{ctx}
{
}

local_context_tree_builder_base::~local_context_tree_builder_base()
{
}

void
local_context_tree_builder_base::visit_val_arg(std::size_t ix)
{
    make_sub_ctx(ix, false);
}

std::unique_ptr<req_visitor_intf>
local_context_tree_builder_base::visit_req_arg(std::size_t ix)
{
    auto sub_ctx{make_sub_ctx(ix, true)};
    return make_sub_builder(*sub_ctx);
}

std::shared_ptr<local_async_context_base>
local_context_tree_builder_base::make_sub_ctx(std::size_t ix, bool is_req)
{
    auto& tree_ctx{ctx_.get_tree_context()};
    auto sub_ctx{make_sub_ctx(tree_ctx, ix, is_req)};
    ctx_.add_sub(ix, sub_ctx);
    register_local_async_ctx(sub_ctx);
    return sub_ctx;
}

proxy_async_tree_context_base::proxy_async_tree_context_base(
    inner_resources& resources, std::string proxy_name)
    : resources_(resources),
      proxy_name_{std::move(proxy_name)},
      ctoken_{csource_.token()},
      logger_{spdlog::get("cradle")}
{
}

proxy_async_context_base::proxy_async_context_base(
    proxy_async_tree_context_base& tree_ctx)
    : tree_ctx_{tree_ctx}, id_{allocate_async_id()}
{
}

cppcoro::task<>
proxy_async_context_base::schedule_after(std::chrono::milliseconds delay)
{
    auto& io_svc{get_resources().the_io_service()};
    co_await io_svc.schedule_after(delay, tree_ctx_.get_cancellation_token());
}

std::size_t
proxy_async_context_base::get_num_subs() const
{
    ensure_subs();
    return subs_.size();
}

async_context_intf&
proxy_async_context_base::get_sub(std::size_t ix)
{
    assert(have_subs_);
    return *subs_[ix];
}

cppcoro::task<async_status>
proxy_async_context_base::get_status_coro()
{
    wait_on_remote_id();
    auto& proxy{get_proxy()};
    co_return proxy.get_async_status(remote_id_);
}

cppcoro::task<void>
proxy_async_context_base::request_cancellation_coro()
{
    tree_ctx_.request_local_cancellation();
    wait_on_remote_id();
    auto& proxy{get_proxy()};
    proxy.request_cancellation(remote_id_);
    co_return;
}

void
proxy_async_context_base::ensure_subs() const
{
    const_cast<proxy_async_context_base*>(this)->ensure_subs_no_const();
}

void
proxy_async_context_base::ensure_subs_no_const()
{
    wait_on_remote_id();
    std::scoped_lock<std::mutex> lock(subs_mutex_);
    if (have_subs_)
    {
        return;
    }
    auto& proxy{get_proxy()};
    // Wait until the get_sub_contexts precondition holds
    wait_until_async_status_matches(
        proxy, remote_id_, subs_available_matcher(tree_ctx_.get_logger()));
    auto specs = proxy.get_sub_contexts(remote_id_);
    for (auto& spec : specs)
    {
        auto [sub_aid, is_req] = spec;
        auto sub_ctx = make_sub_ctx(tree_ctx_, is_req);
        sub_ctx->set_remote_id(sub_aid);
        subs_.push_back(std::move(sub_ctx));
    }
    have_subs_ = true;
}

root_proxy_async_context_base::root_proxy_async_context_base(
    proxy_async_tree_context_base& tree_ctx)
    : proxy_async_context_base{tree_ctx},
      remote_id_future_{remote_id_promise_.get_future()}
{
}

void
root_proxy_async_context_base::finish_remote() noexcept
{
    // Clean up the context tree on the server once per proxy context tree.
    // There must have been a set_remote_id() or fail_remote_id() call for this
    // root context.
    if (remote_id_ != NO_ASYNC_ID)
    {
        // Must not throw
        try
        {
            auto& proxy{get_proxy()};
            proxy.finish_async(remote_id_);
        }
        catch (std::exception& e)
        {
            auto& logger{tree_ctx_.get_logger()};
            try
            {
                logger.error(
                    "root_proxy_async_context_base::finish_remote() caught {}",
                    e.what());
            }
            catch (...)
            {
            }
        }
    }
}

void
root_proxy_async_context_base::set_remote_id(async_id remote_id)
{
    // This could throw std::future_error, but that should be possible only
    // when the caller is misbehaving.
    remote_id_promise_.set_value(remote_id);
}

void
root_proxy_async_context_base::fail_remote_id() noexcept
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
            auto& logger(tree_ctx_.get_logger());
            logger.warn(
                "root_proxy_async_context_base::fail_remote_id caught {}",
                e.what());
        }
        catch (...)
        {
        }
    }
}

// This may cause the current thread to block on getting the result from a
// future.
// TODO Avoid blocking callers by moving to a cppcoro-based interface, and
// rescheduling on a thread from a cppcoro pool before calling anything
// blocking.
// The remote_id would be retrieved from
//   cppcoro::shared_task<async_id>
//   remote_proxy::submit_async(...);
// which would throw on failure.
void
root_proxy_async_context_base::wait_on_remote_id()
{
    if (remote_id_ == NO_ASYNC_ID)
    {
        remote_id_ = remote_id_future_.get();
    }
}

non_root_proxy_async_context_base::non_root_proxy_async_context_base(
    proxy_async_tree_context_base& tree_ctx, bool is_req)
    : proxy_async_context_base{tree_ctx}, is_req_{is_req}
{
}

void
non_root_proxy_async_context_base::set_remote_id(async_id remote_id)
{
    remote_id_ = remote_id;
}

void
non_root_proxy_async_context_base::fail_remote_id() noexcept
{
    // For non-root contexts, remote_id_ is set on object creation
    assert(false);
}

void
non_root_proxy_async_context_base::wait_on_remote_id()
{
    // For non-root contexts, remote_id_ is set on object creation
}

void
register_local_async_ctx(std::shared_ptr<local_async_context_base> ctx)
{
    auto& tree_ctx{ctx->get_tree_context()};
    if (auto* db = get_async_db(tree_ctx))
    {
        db->add(ctx);
    }
}

} // namespace cradle
