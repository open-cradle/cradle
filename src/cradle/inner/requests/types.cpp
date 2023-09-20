#include <atomic>

#include <cradle/inner/requests/types.h>

namespace cradle {

catalog_id::catalog_id()
{
    static std::atomic<wrapped_t> next_dll_id{1};
    wrapped_ = next_dll_id.fetch_add(1, std::memory_order_relaxed);
}

} // namespace cradle
