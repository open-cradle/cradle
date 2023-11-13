#include "make_some_blob_t0_impl.h"
#include <cradle/inner/core/type_interfaces.h>

namespace cradle {

cppcoro::task<blob>
make_test_blob(context_intf& ctx, std::string payload)
{
    co_return make_blob(payload);
}

} // namespace cradle
