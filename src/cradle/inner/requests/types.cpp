#include <atomic>

#include <cradle/inner/requests/types.h>

namespace cradle {

catalog_id
catalog_id::get_static_id()
{
    return catalog_id{STATIC_ID};
}

catalog_id
catalog_id::alloc_dll_id()
{
    static std::atomic<wrapped_t> next_dll_id{MIN_DLL_ID};
    return next_dll_id.fetch_add(1, std::memory_order_relaxed);
}

} // namespace cradle
