#include <stdexcept>

#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/remote/loopback_impl.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/seri_req.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

loopback_service::loopback_service(
    service_config const& config, inner_resources& resources)
    : resources_{resources},
      async_pool_{static_cast<BS::concurrency_t>(config.get_number_or_default(
          loopback_config_keys::ASYNC_CONCURRENCY, 16))}
{
    if (!logger_)
    {
        logger_ = create_logger("loopback");
    }
}

std::string
loopback_service::name() const
{
    return "loopback";
}

spdlog::logger&
loopback_service::get_logger()
{
    return *logger_;
}

serialized_result
loopback_service::resolve_sync(
    remote_context_intf& ctx, std::string domain_name, std::string seri_req)
{
    logger_->debug("resolve_sync({}): request {}", domain_name, seri_req);
    auto local_ctx{ctx.local_clone()};
    auto result = cppcoro::sync_wait(
        resolve_serialized_local(*local_ctx, std::move(seri_req)));
    logger_->debug("response {}", result.value());
    return result;
}

static void
resolve_async(
    loopback_service* loopback,
    std::shared_ptr<local_async_context_intf> actx,
    std::string seri_req)
{
    auto& logger{loopback->get_logger()};
    logger.info("resolve_async start");
    // TODO update status to STARTED or so
    try
    {
        blob res = cppcoro::sync_wait(
                       resolve_serialized_local(*actx, std::move(seri_req)))
                       .value();
        logger.info("resolve_async done: {}", res);
        if (actx->get_status() != async_status::FINISHED)
        {
            logger.error(
                "resolve_async finished but status is {}", actx->get_status());
        }
        actx->set_result(std::move(res));
    }
    catch (async_cancelled const&)
    {
        logger.warn("resolve_async: caught async_cancelled");
        actx->update_status(async_status::CANCELLED);
    }
    catch (std::exception& e)
    {
        logger.warn("resolve_async: caught error {}", e.what());
        actx->update_status_error(e.what());
    }
}

async_id
loopback_service::submit_async(
    remote_context_intf& ctx, std::string domain_name, std::string seri_req)
{
    logger_->info(
        "submit_async {}: {} ...", domain_name, seri_req.substr(0, 10));
    auto actx{
        cast_ctx_to_ref<remote_async_context_intf>(ctx).local_async_clone()};
    resources_.ensure_async_db();
    get_async_db().add(actx);
    // TODO populate actx subs?
    // TODO update status to SUBMITTED
    // This function should return asap, but cppcoro::sync_wait() is blocking,
    // so need to dispatch to another thread.
    async_pool_.push_task(resolve_async, this, actx, seri_req);
    async_id aid = actx->get_id();
    logger_->info("async_id {}", aid);
    return aid;
}

remote_context_spec_list
loopback_service::get_sub_contexts(async_id aid)
{
    logger_->info("handle_get_sub_contexts {}", aid);
    auto actx{get_async_db().find(aid)};
    auto nsubs = actx->get_num_subs();
    logger_->debug("  {} subs", nsubs);
    remote_context_spec_list result;
    for (decltype(nsubs) ix = 0; ix < nsubs; ++ix)
    {
        auto& sub_actx = actx->get_local_sub(ix);
        logger_->debug(
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

async_status
loopback_service::get_async_status(async_id aid)
{
    logger_->info("handle_get_async_status {}", aid);
    auto actx{get_async_db().find(aid)};
    auto status = actx->get_status();
    logger_->info("handle_get_async_status -> {}", status);
    return status;
}

std::string
loopback_service::get_async_error_message(async_id aid)
{
    logger_->info("handle_get_async_error_message {}", aid);
    auto actx{get_async_db().find(aid)};
    auto errmsg = actx->get_error_message();
    logger_->info("handle_get_async_error_message -> {}", errmsg);
    return errmsg;
}

serialized_result
loopback_service::get_async_response(async_id root_aid)
{
    logger_->info("handle_get_async_response {}", root_aid);
    auto actx{get_async_db().find(root_aid)};
    return serialized_result{actx->get_result()};
}

void
loopback_service::request_cancellation(async_id aid)
{
    logger_->info("handle_request_cancellation {}", aid);
    auto actx{get_async_db().find(aid)};
    actx->request_cancellation();
}

void
loopback_service::finish_async(async_id root_aid)
{
    logger_->info("handle_finish_async {}", root_aid);
    get_async_db().remove_tree(root_aid);
}

async_db&
loopback_service::get_async_db()
{
    auto adb{resources_.get_async_db()};
    if (!adb)
    {
        throw std::logic_error{"loopback service has no async_db"};
    }
    return *adb;
}

void
register_loopback_service(
    service_config const& config, inner_resources& resources)
{
    auto proxy{std::make_shared<loopback_service>(config, resources)};
    register_proxy(proxy);
}

} // namespace cradle
