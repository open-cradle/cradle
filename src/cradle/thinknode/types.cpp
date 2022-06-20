#include <cradle/thinknode/types.hpp>
#include <cradle/typing/service/core.h>

namespace cradle {

// TODO refactor .h(pp) files and inline this function
inner_service_core&
thinknode_request_context::get_service()
{
    return service;
}

} // namespace cradle
