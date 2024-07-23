#include <algorithm>
#include <utility>

#include "make_test_blob.h"
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

blob
make_test_blob(
    local_context_intf& ctx,
    std::string const& contents,
    bool use_shared_memory)
{
    auto size = contents.size();
    auto owner = ctx.make_data_owner(size, use_shared_memory);
    auto* data = reinterpret_cast<char*>(owner->data());
    std::copy_n(contents.data(), size, data);
    ctx.on_value_complete();
    return blob{std::move(owner), as_bytes(data), size};
}

} // namespace cradle
