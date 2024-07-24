#include <cradle/inner/core/exception.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/resolve/creq_context.h>
#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

creq_context::creq_context(
    inner_resources& resources,
    std::shared_ptr<spdlog::logger> logger,
    std::string domain_name)
    : resources_{resources},
      logger_{logger},
      domain_name_{std::move(domain_name)},
      proxy_{resources.alloc_contained_proxy(logger)}
{
}

creq_context::~creq_context()
{
    finish_remote();
    resources_.free_contained_proxy(std::move(proxy_), succeeded_);
}

void
creq_context::mark_succeeded()
{
    succeeded_ = true;
}

cppcoro::task<>
creq_context::schedule_after(std::chrono::milliseconds delay)
{
    auto& io_svc{resources_.the_io_service()};
    co_await io_svc.schedule_after(delay);
}

service_config
creq_context::make_config(bool need_record_lock) const
{
    // Config for the rpclib server
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, domain_name_},
    };
    update_config_map_with_test_params(config_map);
    return service_config{config_map};
}

async_id
creq_context::get_id() const
{
    throw not_implemented_error("creq_context::get_id()");
}

bool
creq_context::is_req() const
{
    throw not_implemented_error("creq_context::is_req()");
}

std::size_t
creq_context::get_num_subs() const
{
    throw not_implemented_error("creq_context::get_num_subs()");
}

async_context_intf&
creq_context::get_sub(std::size_t ix)
{
    throw not_implemented_error("creq_context::get_sub(std::size_t");
}

cppcoro::task<async_status>
creq_context::get_status_coro()
{
    throw not_implemented_error("creq_context::get_status_coro()");
}

/**
 * This initiates a controlled cancellation / shutdown of the contained
 * process:
 * - The client sends a cancellation request to the process.
 * - The function being resolved by the process polls the cancellation request
 *   status.
 * - The function throws an async_cancelled exception.
 * - The process's rpclib handler sets the context's status to
 *   async_status::CANCELLED.
 * - The client is in a resolve_request() call, polling the process's context
 *   status.
 * - The client's resolve_request logic detects that the process was
 *   cancelled, and itself throws an async_cancelled exception.
 * - The client's creq_controller object is destroyed, causing it to terminate
 *   the process.
 *
 * Should the cancellation request coincide with a crash of the process, then
 * the resolve_request's polling logic will run into a timeout, throwing an
 * exception from the resolve_request() call.
 * Should the process's function enter a hangup, then the client's
 * resolve_request call hangs as well. This is not different from a local
 * resolution.
 */
cppcoro::task<void>
creq_context::request_cancellation_coro()
{
    std::scoped_lock lock{mutex_};
    cancelled_ = true;
    // If remote_id_ is set later, the cancellation request to the proxy
    // happens at that time, in set_remote_id().
    if (remote_id_ != NO_ASYNC_ID)
    {
        proxy_->request_cancellation(remote_id_);
    }
    co_return;
}

void
creq_context::finish_remote() noexcept
{
    // Clean up the context tree on the server for this context.
    if (remote_id_ != NO_ASYNC_ID)
    {
        // Must not throw
        try
        {
            proxy_->finish_async(remote_id_);
        }
        catch (std::exception& e)
        {
            try
            {
                logger_->error(
                    "creq_context::finish_remote() caught {}", e.what());
            }
            catch (...)
            {
            }
        }
    }
}

void
creq_context::set_remote_id(async_id remote_id)
{
    logger_->debug("creq_context::set_remote_id({})", remote_id);
    std::scoped_lock lock{mutex_};
    remote_id_ = remote_id;
    if (cancelled_)
    {
        logger_->debug("  already cancelled - propagating to proxy");
        proxy_->request_cancellation(remote_id);
    }
}

void
creq_context::fail_remote_id() noexcept
{
    logger_->debug("creq_context::fail_remote_id()");
    // This should cause blocking async_context_intf calls that need the
    // remote_id to unblock and fail.
    // The only async_context_intf function implemented in this class is
    // request_cancellation_coro(), which is not blocking.
    // The caller will throw and abort the resolve_request() call; there is
    // nothing left to do here.
}

void
creq_context::make_introspective()
{
    throw not_implemented_error("creq_context::make_introspective()");
}

bool
creq_context::introspective() const
{
    return false;
}

void
creq_context::throw_if_cancelled()
{
    logger_->debug("creq_context::throw_if_cancelled()");
    if (cancelled_)
    {
        logger_->debug("  already cancelled - throwing");
        throw async_cancelled{"creq_context cancelled"};
    }
}

} // namespace cradle
