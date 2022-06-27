#include <cradle/inner/service/internals.h>
#include <cradle/thinknode/types.hpp>
#include <cradle/typing/service/core.h>

namespace cradle {

inner_service_core&
thinknode_request_context::get_service()
{
    return service;
}

immutable_cache&
thinknode_request_context::get_cache()
{
    return service.inner_internals().cache;
}

} // namespace cradle
