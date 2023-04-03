#include <cassert>
#include <future>
#include <stdexcept>

#include <cppcoro/operation_cancelled.hpp>
#include <cppcoro/sync_wait.hpp>
#include <rpc/this_handler.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/seri_req.h>
#include <cradle/rpclib/server/handlers.h>
#include <cradle/typing/service/core.h>

namespace cradle {

rpclib_handler_context::rpclib_handler_context(
    service_core& service, spdlog::logger& logger)
    : service_{service}, logger_{logger}
{
}

// [[noreturn]]
// Throws something that is handled inside the rpclib library
static void
handle_exception(rpclib_handler_context& hctx, std::exception& e)
{
    auto& logger{hctx.logger()};
    logger.error("caught {}", e.what());
    rpc::this_handler().respond_error(e.what());
}

cppcoro::task<rpclib_response>
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string domain_name,
    std::string seri_req)
try
{
    blob result;
    auto& service{hctx.service()};
    try
    {
        auto& logger{hctx.logger()};
        logger.info("resolve_sync {}: {}", domain_name, seri_req);
        auto dom = find_domain(domain_name);
        auto ctx{dom->make_local_context(service)};
        auto seri_result
            = co_await resolve_serialized_request(*ctx, std::move(seri_req));
        // TODO try to get rid of .value()
        result = seri_result.value();
        logger.info("result {}", result);
    }
    catch (std::exception& e)
    {
        handle_exception(hctx, e);
    }
    // TODO if the result references blob files, then create a response_id
    // uniquely identifying the set of those files
    static uint32_t response_id = 0;
    response_id += 1;
    co_return rpclib_response{response_id, std::move(result)};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    co_return rpclib_response{};
}

cppcoro::task<void>
handle_ack_response(rpclib_handler_context& hctx, int response_id)
try
{
    auto& logger{hctx.logger()};
    logger.info("ack_response {}", response_id);
    // TODO release the temporary lock on the blob files referenced in
    // response #response_id
    co_return;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

cppcoro::task<void>
handle_mock_http(rpclib_handler_context& hctx, std::string const& body)
try
{
    auto& session = enable_http_mocking(hctx.service());
    session.set_canned_response(make_http_200_response(body));
    co_return;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

// Compilers refuse calls with rpclib_handler_context&
static blob
resolve_async(
    rpclib_handler_context* hctx,
    std::shared_ptr<local_async_context_intf> actx,
    std::string seri_req)
{
    auto& logger{hctx->logger()};
    logger.info("resolve_async start");
    blob res;
    // TODO update status to STARTED or so
    try
    {
        res = cppcoro::sync_wait(
                  resolve_serialized_request(*actx, std::move(seri_req)))
                  .value();
    }
    catch (cppcoro::operation_cancelled const&)
    {
        logger.warn("resolve_async: caught operation_cancelled");
        actx->update_status(async_status::CANCELLED);
        // Re-throwing causes the exception to be stored with the future
        throw;
    }
    catch (std::exception& e)
    {
        logger.warn("resolve_async: caught error {}", e.what());
        actx->update_status(async_status::ERROR);
        // Re-throwing causes the exception to be stored with the future
        throw;
    }
    logger.info("resolve_async done: {}", res);
    if (actx->get_status() != async_status::FINISHED)
    {
        logger.error(
            "resolve_async finished but status is {}", actx->get_status());
    }
    return res;
}

async_id
handle_submit_async(
    rpclib_handler_context& hctx,
    std::string const& domain_name,
    std::string const& seri_req)
try
{
    auto& service{hctx.service()};
    auto& logger{hctx.logger()};
    // logger.info("submit_async {}: {}", domain_name, seri_req);
    logger.info(
        "submit_async {}: {} ...", domain_name, seri_req.substr(0, 10));
    auto dom = find_domain(domain_name);
    auto ctx{dom->make_local_context(service)};
    auto actx = to_local_async_context_intf(ctx);
    // TODO what if actx == nullptr?
    hctx.get_async_db().add(actx);
    // TODO update status to SUBMITTED
    std::future<blob> my_future
        = hctx.async_pool().submit(resolve_async, &hctx, actx, seri_req);
    actx->set_future(std::move(my_future));
    async_id aid = actx->get_id();
    logger.info("async_id {}", aid);
    return aid;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return async_id{};
}

remote_context_spec_list
handle_get_sub_contexts(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_get_sub_contexts {}", aid);
    auto actx{db.find(aid)};
    auto nsubs = actx->get_num_subs();
    logger.debug("  {} subs", nsubs);
    remote_context_spec_list result;
    for (decltype(nsubs) ix = 0; ix < nsubs; ++ix)
    {
        auto& sub_actx = actx->get_sub(ix);
        logger.debug(
            "  sub {}: id {} ({}) {}",
            ix,
            sub_actx.get_id(),
            sub_actx.is_req() ? "REQ" : "VAL",
            sub_actx.get_status());
        result.push_back(
            remote_context_spec{sub_actx.get_id(), sub_actx.is_req()});
    }
    return result;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return remote_context_spec_list{};
}

int
handle_get_async_status(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_get_async_status {}", aid);
    auto actx{db.find(aid)};
    auto status = actx->get_status();
    logger.info("handle_get_async_status -> {}", status);
    return static_cast<int>(status);
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

rpclib_response
handle_get_async_response(rpclib_handler_context& hctx, async_id root_aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_get_async_response {}", root_aid);
    auto actx{db.find(root_aid)};
    // TODO response_id
    uint32_t response_id = 0;
    return rpclib_response{response_id, actx->get_value()};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return rpclib_response{};
}

int
handle_request_cancellation(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_request_cancellation {}", aid);
    auto actx{db.find(aid)};
    actx->request_cancellation();
    return int{};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

int
handle_finish_async(rpclib_handler_context& hctx, async_id root_aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_finish_async {}", root_aid);
    db.remove_tree(root_aid);
    return int{};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

} // namespace cradle
