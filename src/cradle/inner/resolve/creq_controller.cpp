#include <stdexcept>

#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/creq_context.h>
#include <cradle/inner/resolve/creq_controller.h>
#include <cradle/inner/resolve/remote.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

namespace {

} // namespace

creq_controller::creq_controller(std::string dll_dir, std::string dll_name)
    : dll_dir_{std::move(dll_dir)},
      dll_name_{std::move(dll_name)},
      logger_{ensure_logger("creq")}
{
}

creq_controller::~creq_controller() = default;

cppcoro::task<serialized_result>
creq_controller::resolve(local_context_intf& ctx, std::string seri_req)
{
    auto& resources = ctx.get_resources();
    resources.increase_num_contained_calls();

    // Create a new remote/async context, sharing resources and domain with
    // the original context. The original context could be sync or async, but
    // it will be local; even if it's also remote (and it must be for providing
    // a domain name), its proxy cannot be used, so a new context is needed.
    // Note that seri_resp refers to a deserialization observer which is the
    // rpclib_client_impl owned by ctx_, so ctx_'s lifetime should be at least
    // seri_resp's one.
    auto* actx = cast_ctx_to_ptr<local_async_context_intf>(ctx);
    ctx_ = std::make_shared<creq_context>(
        resources, logger_, ctx.domain_name());
    if (actx)
    {
        // Allow cancellation requests on actx to propagate to ctx_
        actx->set_delegate(ctx_);
        // Act on a cancellation request issued while we were starting up
        ctx_->throw_if_cancelled();
    }
    auto& proxy{ctx_->get_proxy()};
    proxy.load_shared_library(dll_dir_, dll_name_);
    auto seri_resp = resolve_remote(*ctx_, std::move(seri_req), nullptr);
    ctx_->mark_succeeded();
    co_return seri_resp;
}

} // namespace cradle
