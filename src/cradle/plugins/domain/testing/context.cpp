#include <mutex>
#include <stdexcept>

#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

// TODO make everything thread-safe

namespace cradle {

testing_request_context::testing_request_context(
    inner_resources& service, tasklet_tracker* tasklet, bool remotely)
    : service{service}, remotely_{remotely}
{
    if (tasklet)
    {
        tasklets_.push_back(tasklet);
    }
}

inner_resources&
testing_request_context::get_resources()
{
    return service;
}

immutable_cache&
testing_request_context::get_cache()
{
    return service.memory_cache();
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

void
testing_request_context::proxy_name(std::string const& name)
{
    proxy_name_ = name;
}

std::string const&
testing_request_context::proxy_name() const
{
    if (proxy_name_.empty())
    {
        throw std::logic_error(
            "testing_request_context::proxy_name(): name not set");
    }
    return proxy_name_;
}

std::shared_ptr<remote_context_intf>
testing_request_context::local_clone() const
{
    return std::make_shared<testing_request_context>(service, nullptr);
}

std::shared_ptr<blob_file_writer>
testing_request_context::make_blob_file_writer(size_t size)
{
    return service.make_blob_file_writer(size);
}

namespace {

static std::mutex allocate_async_id_mutex;

static async_id
allocate_async_id()
{
    std::scoped_lock lock{allocate_async_id_mutex};
    static async_id next_id{1};
    return next_id++;
}

} // namespace

local_atst_context::local_atst_context(
    std::shared_ptr<local_atst_tree_context> tree_ctx, bool is_req)
    : tree_ctx_{std::move(tree_ctx)},
      is_req_{is_req},
      id_{allocate_async_id()},
      status_{is_req ? async_status::CREATED : async_status::FINISHED}
{
    auto logger = spdlog::get("cradle");
    logger->info(
        "local_atst_context {} ({}): created, status {}",
        id_,
        is_req ? "REQ" : "VAL",
        status_);
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

void
local_atst_context::add_sub(
    std::size_t ix, std::shared_ptr<local_atst_context> sub)
{
    assert(ix == subs_.size());
    subs_.push_back(std::move(sub));
}

void
local_atst_context::set_future(std::future<blob> future)
{
    future_ = std::move(future);
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
        auto logger = spdlog::get("cradle");
        logger->info(
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
    auto logger = spdlog::get("cradle");
    logger->info(
        "local_atst_context::update_status {}: {} -> {}",
        id_,
        status_,
        status);
    // Invariant: if this status is FINISHED, then all subs too
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
    auto logger = spdlog::get("cradle");
    logger->info(
        "local_atst_context::update_status_error {}: {} -> ERROR: {}",
        id_,
        status_,
        errmsg);
    status_ = async_status::ERROR;
    errmsg_ = errmsg;
}

// TODO check status
// - FINISHED: proceed
// - ERROR: proceed?
// - other: not allowed - error
blob
local_atst_context::get_value()
{
    return future_.get();
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
        throw async_cancelled{fmt::format("async {} cancelled", id_)};
    }
}

local_atst_context_tree_builder::local_atst_context_tree_builder(
    local_atst_context& actx)
    : actx_{actx}
{
}

void
local_atst_context_tree_builder::visit_val_arg(std::size_t ix)
{
    auto tree_ctx{actx_.get_tree_context()};
    auto sub_ctx = std::make_shared<local_atst_context>(tree_ctx, false);
    actx_.add_sub(ix, sub_ctx);
    if (auto* db = tree_ctx->get_async_db())
    {
        db->add(sub_ctx);
    }
}

std::unique_ptr<req_visitor_intf>
local_atst_context_tree_builder::visit_req_arg(std::size_t ix)
{
    auto tree_ctx{actx_.get_tree_context()};
    auto sub_ctx = std::make_shared<local_atst_context>(tree_ctx, true);
    auto sub_builder{
        std::make_unique<local_atst_context_tree_builder>(*sub_ctx)};
    actx_.add_sub(ix, sub_ctx);
    if (auto* db = tree_ctx->get_async_db())
    {
        db->add(sub_ctx);
    }
    return sub_builder;
}

proxy_atst_tree_context::proxy_atst_tree_context(
    inner_resources& inner, std::string const& proxy_name)
    : inner_(inner), proxy_name_{proxy_name}
{
}

remote_proxy&
proxy_atst_tree_context::get_proxy() const
{
    if (!proxy_)
    {
        proxy_ = std::make_unique<remote_proxy*>(&find_proxy(proxy_name_));
    }
    return **proxy_;
}

proxy_atst_context::proxy_atst_context(
    std::shared_ptr<proxy_atst_tree_context> tree_ctx,
    bool is_root,
    bool is_req)
    : tree_ctx_{std::move(tree_ctx)},
      is_root_{is_root},
      is_req_{is_req},
      id_{allocate_async_id()}
{
}

proxy_atst_context::~proxy_atst_context()
{
    // Clean up the context tree on the server once per proxy context tree
    if (is_root_ && remote_id_ != NO_ASYNC_ID)
    {
        // Destructor must not throw
        try
        {
            auto& proxy{get_proxy()};
            cppcoro::sync_wait(proxy.finish_async(remote_id_));
        }
        catch (std::exception& e)
        {
            // TODO? put logger in proxy_atst_tree_context
            auto logger = spdlog::get("cradle");
            logger->error("~proxy_atst_context() caught {}", e.what());
        }
    }
}

std::string const&
proxy_atst_context::proxy_name() const
{
    return tree_ctx_->get_proxy_name();
}

std::shared_ptr<local_async_context_intf>
proxy_atst_context::local_async_clone() const
{
    return std::make_shared<local_atst_context>(
        std::make_shared<local_atst_tree_context>(
            tree_ctx_->get_inner_resources()),
        is_req_);
}

std::shared_ptr<remote_context_intf>
proxy_atst_context::local_clone() const
{
    throw not_implemented_error{"proxy_atst_context::local_clone()"};
}

static cppcoro::task<async_status>
async_status_value_coro(async_status status)
{
    co_return status;
}

cppcoro::task<async_status>
proxy_atst_context::get_status_coro()
{
    auto& proxy{get_proxy()};
    if (remote_id_ == NO_ASYNC_ID)
    {
        // Assume remote_id will soon be received
        return async_status_value_coro(async_status::CREATED);
    }
    return proxy.get_async_status(remote_id_);
}

static cppcoro::task<void>
do_nothing_coro()
{
    co_return;
}

cppcoro::task<void>
proxy_atst_context::request_cancellation_coro()
{
    auto& proxy{get_proxy()};
    if (remote_id_ == NO_ASYNC_ID)
    {
        cancellation_pending_ = true;
        return do_nothing_coro();
    }
    return proxy.request_cancellation(remote_id_);
}

remote_async_context_intf&
proxy_atst_context::add_sub(async_id sub_remote_id, bool is_req)
{
    auto sub = std::make_unique<proxy_atst_context>(tree_ctx_, false, is_req);
    sub->set_remote_id(sub_remote_id);
    auto& result{*sub};
    subs_.push_back(std::move(sub));
    return result;
}

async_id
proxy_atst_context::get_remote_id()
{
    if (remote_id_ == NO_ASYNC_ID)
    {
        throw std::logic_error("proxy_atst_context::get_remote_id(): not set");
    }
    return remote_id_;
}

// Throws if proxy was not registered
remote_proxy&
proxy_atst_context::get_proxy()
{
    return tree_ctx_->get_proxy();
}

} // namespace cradle
