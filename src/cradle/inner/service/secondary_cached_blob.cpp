#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/service/secondary_cached_blob.h>

namespace cradle {

cppcoro::task<blob>
secondary_cached_blob(
    inner_resources& resources,
    captured_id id_key,
    std::function<cppcoro::task<blob>()> create_task)
{
    std::string key{get_unique_string(*id_key)};
    auto& cache = resources.secondary_cache();
    auto result = co_await cache.read(key);
    if (!result.data())
    {
        result = co_await create_task();
        co_await cache.write(key, result);
    }
    co_return result;
}

} // namespace cradle
