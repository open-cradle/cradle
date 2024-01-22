#include <string>

#include <benchmark/benchmark.h>
#include <fmt/format.h>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

namespace cradle {

void
BM_disk_cache_read(benchmark::State& state)
{
    std::string directory{"disk_cache"};
    reset_directory(directory);
    ll_disk_cache_config config;
    config.directory = directory;
    ll_disk_cache cache{config};
    constexpr int num_items = 100;

    std::vector<std::string> keys;
    for (int i = 0; i < num_items; ++i)
    {
        auto key{get_unique_string_tmpl(fmt::format("key{}", i))};
        auto value{make_blob(fmt::format("value{}", i))};
        auto digest{get_unique_string_tmpl(value)};
        cache.insert(key, digest, value);
        keys.push_back(key);
    }

    for (auto _ : state)
    {
        for (int i = 0; i < num_items; ++i)
        {
            // Perform a single SQL query, on the AC
            cache.look_up_ac_id(keys[i]);
        }
        cache.flush_ac_usage(true);
    }
}

BENCHMARK(BM_disk_cache_read);

} // namespace cradle
