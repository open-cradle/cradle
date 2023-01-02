#ifndef CRADLE_INNER_SERVICE_SERI_REGISTRY_H
#define CRADLE_INNER_SERVICE_SERI_REGISTRY_H

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/seri_catalog.h>

namespace cradle {

/**
 * Registers a resolver from a template/sample request object.
 *
 * The resolver will be able to resolve serialized requests that are similar
 * to the template one; different arguments are allowed, but otherwise the
 * request should be identical to the template.
 *
 * Context at resolution time should equal Ctx.
 */
template<Context Ctx, Request Req>
void
register_seri_resolver(Req const& req)
{
    seri_catalog::instance().register_resolver<Ctx, Req>(req.get_uuid().str());
}

} // namespace cradle

#endif
