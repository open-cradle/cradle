#include <cradle/plugins/pool/local/pool_registration.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include "../../../support/inner_service.h"
#include "../../../support/request.h"
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_spec_builder.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/pool/pool_config_keys.h>
#include <cradle/inner/pool/pool_intf.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/resolve_pool.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/inner/storage/storage_config_keys.h>
#include <cradle/inner/storage/storage_registration.h>
#include <cradle/plugins/secondary_cache/local/durable_storage_registration.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

using namespace cradle;

namespace {

char const tag[] = "[plugins][pool][pool_registration]";

// The non-introspective, no-retrier coro request shape the worker can
// reassemble from a provider identity and serialized inputs. All pooled leaves
// use this shape.
using leaf_props
    = request_props<caching_level_type::none, request_function_t::coro>;

// Counts how many times the pooled provider actually executed (in a worker).
// A repeated resolve that hits the action cache must not increment it.
std::atomic<int> pooled_run_count{0};

// Counts how many times the non-pooled (inline) provider executed.
std::atomic<int> inline_run_count{0};

// A pooled leaf provider whose result reflects both of its inputs: the exact
// content of the large blob input and the value of the small integer input.
cppcoro::task<blob>
pooled_concat(context_intf&, blob large, int small_value)
{
    ++pooled_run_count;
    co_return make_blob(
        fmt::format("pooled[{}]:{}", to_string(large), small_value));
}

// A non-pooled leaf resolved on the unchanged inline path. It is deliberately
// not registered with any resolver: if it were routed to a pool it could not
// be reassembled, so a successful resolve proves it took the inline path.
cppcoro::task<blob>
inline_tag(context_intf&, int n)
{
    ++inline_run_count;
    co_return make_blob(fmt::format("inline:{}", n));
}

// A non-pooled root that combines a pooled subrequest result with a non-pooled
// subrequest result into one value.
cppcoro::task<blob>
combine_root(context_intf&, blob pooled_result, blob inline_result)
{
    co_return make_blob(fmt::format(
        "{}|{}", to_string(pooled_result), to_string(inline_result)));
}

// Removes a test-created directory on scope exit so a durable-storage test
// leaves no artifacts behind even if an assertion aborts the body.
struct temp_directory
{
    std::string path;

    explicit temp_directory(std::string dir) : path{std::move(dir)}
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~temp_directory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// Builds resources whose config selects the in-memory storage backends and the
// local pool, then runs the real storage and pool registration dispatchers.
// The pool binds to the default in-memory instances and classifies inputs
// against the given boundary (read from config by the resolve branch).
std::unique_ptr<inner_resources>
make_pooled_resources(std::string const& pool_name, std::size_t boundary)
{
    service_config_map config{make_inner_tests_config().get_config_map()};
    config[storage_config_keys::CAS_FACTORY]
        = storage_config_values::MEMORY_CAS;
    config[storage_config_keys::AC_FACTORY] = storage_config_values::MEMORY_AC;
    config[storage_config_keys::MUTABLE_STORE_FACTORY]
        = storage_config_values::MEMORY_MUTABLE_STORE;
    config[pool_config_keys::FACTORY] = pool_config_values::LOCAL_POOL;
    config[pool_config_keys::NAME] = pool_name;
    config[pool_config_keys::INPUT_SIZE_BOUNDARY] = boundary;

    auto resources{std::make_unique<inner_resources>(service_config{config})};
    register_storage_from_config(*resources);
    register_local_pool_from_config(*resources);
    return resources;
}

// Config selecting the durable (on-disk) backends against one shared directory
// plus the local pool bound to those durable defaults.
service_config_map
make_durable_config(
    std::string const& dir, std::string const& pool_name, std::size_t boundary)
{
    service_config_map config;
    config[storage_config_keys::CAS_FACTORY]
        = durable_storage_config_values::DISK_CAS;
    config[storage_config_keys::AC_FACTORY]
        = durable_storage_config_values::DISK_AC;
    config[storage_config_keys::MUTABLE_STORE_FACTORY]
        = durable_storage_config_values::DISK_MUTABLE_STORE;
    config[local_disk_cache_config_keys::DIRECTORY] = dir;
    config[pool_config_keys::FACTORY] = pool_config_values::LOCAL_POOL;
    config[pool_config_keys::NAME] = pool_name;
    config[pool_config_keys::INPUT_SIZE_BOUNDARY] = boundary;
    return config;
}

// A large blob input (above the small test boundary; conveyed by digest) and a
// small integer input (below the boundary; conveyed inline).
blob
large_input()
{
    return make_blob(std::string(64, 'x'));
}

} // namespace

TEST_CASE(
    "register_local_pool_from_config selects and binds a pool by name", tag)
{
    // A pool is configured through service_config, bound to explicitly named
    // CAS / AC / mutable-store instances, and exposes those same instances.
    service_config_map config;
    config[pool_config_keys::FACTORY] = pool_config_values::LOCAL_POOL;
    config[pool_config_keys::NAME] = std::string{"named_pool"};
    config[pool_config_keys::CAS_STORE] = std::string{"cas_a"};
    config[pool_config_keys::AC_STORE] = std::string{"ac_a"};
    config[pool_config_keys::MUTABLE_STORE] = std::string{"mut_a"};

    inner_resources resources{service_config{config}};

    auto cas{make_memory_cas("cas_a")};
    auto* cas_ptr{cas.get()};
    resources.set_cas_store(std::move(cas), true);
    auto ac{make_memory_ac("ac_a")};
    auto* ac_ptr{ac.get()};
    resources.set_ac_store(std::move(ac), true);
    auto mut{make_memory_mutable_store("mut_a")};
    auto* mut_ptr{mut.get()};
    resources.set_mutable_store(std::move(mut), true);

    register_local_pool_from_config(resources);

    // The default pool and the pool looked up by name are the one registered
    // instance, reporting the configured name.
    REQUIRE(resources.pool().name() == "named_pool");
    REQUIRE(&resources.pool("named_pool") == &resources.pool());

    // The pool refers to the exact named storage instances, not copies.
    REQUIRE(&resources.pool().cas() == cas_ptr);
    REQUIRE(&resources.pool().ac() == ac_ptr);
    REQUIRE(&resources.pool().mutable_store() == mut_ptr);
}

TEST_CASE(
    "register_local_pool_from_config rejects an unrecognized factory", tag)
{
    service_config_map config;
    config[pool_config_keys::FACTORY] = std::string{"no_such_pool"};
    inner_resources resources{service_config{config}};

    REQUIRE_THROWS_AS(
        register_local_pool_from_config(resources), config_error);
}

TEST_CASE(
    "register_local_pool_from_config registers nothing without a factory", tag)
{
    // With no pool/factory key the dispatcher is a no-op, so no default pool
    // exists to look up afterward.
    service_config_map config;
    inner_resources resources{service_config{config}};

    register_local_pool_from_config(resources);

    REQUIRE_THROWS_AS(resources.pool(), std::logic_error);
}

TEST_CASE("a mixed pooled and non-pooled tree resolves end to end", tag)
{
    // A small boundary keeps the large blob input above it (digest-conveyed)
    // and the integer input below it (inline).
    std::size_t const boundary{16};
    auto resources{make_pooled_resources("local", boundary)};
    pooled_run_count = 0;
    inline_run_count = 0;

    blob const big{large_input()};
    int const small_value{7};

    // The pooled leaf and the resolver registered for its uuid.
    seri_catalog cat{resources->get_seri_registry()};
    auto pooled_req{rq_function(
        leaf_props{request_uuid{"pool_registration/pooled_concat"}},
        pooled_concat,
        big,
        small_value)};
    cat.register_resolver(pooled_req);
    pooled_req.set_pool_name(std::optional<std::string>{"local"});

    // The non-pooled inline leaf, and a non-pooled root combining both.
    auto inline_req{rq_function(
        leaf_props{request_uuid{"pool_registration/inline_tag"}},
        inline_tag,
        3)};
    auto root_req{rq_function(
        leaf_props{request_uuid{"pool_registration/combine_root"}},
        combine_root,
        pooled_req,
        inline_req)};

    non_caching_request_resolution_context ctx{*resources};
    blob const result{cppcoro::sync_wait(resolve_request(ctx, root_req))};

    // The whole tree resolves to the expected combined value.
    std::string const expected_pooled{
        fmt::format("pooled[{}]:{}", to_string(big), small_value)};
    std::string const expected{fmt::format("{}|inline:3", expected_pooled)};
    REQUIRE(to_string(result) == expected);

    // The pooled leaf ran as a job and the non-pooled leaf ran inline.
    REQUIRE(pooled_run_count == 1);
    REQUIRE(inline_run_count == 1);

    // The large input's content is present in the pool CAS by its digest.
    digest const big_digest{digest_of(serialize_value(big, true))};
    REQUIRE(cppcoro::sync_wait(resources->pool().cas().exists(big_digest)));

    // The pooled leaf's request-key -> digest association appeared in the AC.
    request_key const pooled_key{
        get_unique_string(*pooled_req.get_captured_id())};
    REQUIRE(cppcoro::sync_wait(resources->pool().ac().get(pooled_key))
                .has_value());
}

TEST_CASE("a repeated pooled leaf is satisfied from the action cache", tag)
{
    std::size_t const boundary{16};
    auto resources{make_pooled_resources("local", boundary)};
    pooled_run_count = 0;

    blob const big{large_input()};

    seri_catalog cat{resources->get_seri_registry()};
    auto pooled_req{rq_function(
        leaf_props{request_uuid{"pool_registration/ac_hit"}},
        pooled_concat,
        big,
        5)};
    cat.register_resolver(pooled_req);
    pooled_req.set_pool_name(std::optional<std::string>{"local"});

    non_caching_request_resolution_context ctx{*resources};

    blob const first{cppcoro::sync_wait(resolve_request(ctx, pooled_req))};
    REQUIRE(pooled_run_count == 1);

    // Resolving the same leaf again returns the identical result without
    // running the provider a second time (the AC-hit short-circuit).
    blob const second{cppcoro::sync_wait(resolve_request(ctx, pooled_req))};
    REQUIRE(to_string(second) == to_string(first));
    REQUIRE(pooled_run_count == 1);

    // The association that satisfied the second resolve is present in the AC.
    request_key const pooled_key{
        get_unique_string(*pooled_req.get_captured_id())};
    REQUIRE(cppcoro::sync_wait(resources->pool().ac().get(pooled_key))
                .has_value());
}

TEST_CASE(
    "pooled results survive a recreate at the same durable location", tag)
{
    std::size_t const boundary{16};
    temp_directory const store_dir{"pool_registration_durable"};
    std::string const pool_name{"durable_pool"};

    blob const big{large_input()};
    int const small_value{9};
    std::string const expected{
        fmt::format("pooled[{}]:{}", to_string(big), small_value)};

    // The leaf whose completed result must outlive a teardown/recreate. Its
    // request key is identity-derived, so it is identical across sessions.
    auto pooled_req{rq_function(
        leaf_props{request_uuid{"pool_registration/durable_leaf"}},
        pooled_concat,
        big,
        small_value)};
    pooled_req.set_pool_name(std::optional<std::string>{pool_name});
    request_key const pooled_key{
        get_unique_string(*pooled_req.get_captured_id())};

    pooled_run_count = 0;

    job_id run_id;
    digest result_digest;

    // Session one: run the leaf as a job on the durable-backed pool to a
    // successful terminal record, then destroy the resources.
    {
        inner_resources resources{service_config{
            make_durable_config(store_dir.path, pool_name, boundary)}};
        register_local_durable_storage_from_config(resources);
        register_local_pool_from_config(resources);

        seri_catalog cat{resources.get_seri_registry()};
        cat.register_resolver(pooled_req);

        non_caching_request_resolution_context ctx{resources};
        auto& pool{resources.pool()};

        std::vector<blob> inputs;
        inputs.push_back(serialize_value(big, true));
        inputs.push_back(serialize_value(small_value, true));
        auto spec{cppcoro::sync_wait(build_job_spec(
            pool.cas(),
            pooled_req.get_essentials()->uuid_str,
            pooled_key,
            context_id{},
            std::move(inputs),
            boundary))};

        run_id = cppcoro::sync_wait(pool.submit(std::move(spec)));
        job_record const record{
            cppcoro::sync_wait(observe_until_terminal(ctx, pool, run_id))};
        REQUIRE(record.status == job_status::succeeded);
        result_digest = record.output_digest;
    }
    REQUIRE(pooled_run_count == 1);

    // Session two: fresh resources at the same location re-register the
    // durable storage and the pool.
    {
        inner_resources resources{service_config{
            make_durable_config(store_dir.path, pool_name, boundary)}};
        register_local_durable_storage_from_config(resources);
        register_local_pool_from_config(resources);

        seri_catalog cat{resources.get_seri_registry()};
        cat.register_resolver(pooled_req);

        // The result content survived in the CAS. GCC emits a false-positive
        // -Wmaybe-uninitialized for the optional<blob> returned via the
        // coroutine; suppress it locally (clang lacks this warning).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
        std::optional<blob> const content{
            cppcoro::sync_wait(resources.cas_store().get(result_digest))};
        REQUIRE(content.has_value());
        REQUIRE(to_string(deserialize_value<blob>(*content)) == expected);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

        // The request-key -> digest association survived in the AC.
        std::optional<digest> const assoc{
            cppcoro::sync_wait(resources.ac_store().get(pooled_key))};
        REQUIRE(assoc.has_value());
        REQUIRE(*assoc == result_digest);

        // The last-recorded job status survived in the mutable store.
        std::optional<mutable_value> const stored{cppcoro::sync_wait(
            resources.mutable_store().get(job_record_key(run_id)))};
        REQUIRE(stored.has_value());
        REQUIRE(
            deserialize_job_record(*stored).status == job_status::succeeded);

        // Re-submitting the same request observes the AC hit and returns the
        // persisted result rather than re-running the provider.
        non_caching_request_resolution_context ctx{resources};
        blob const result{
            cppcoro::sync_wait(resolve_request(ctx, pooled_req))};
        REQUIRE(to_string(result) == expected);
        REQUIRE(pooled_run_count == 1);
    }
}

TEST_CASE("concurrent pooled jobs all complete with correct results", tag)
{
    std::size_t const boundary{16};
    auto resources{make_pooled_resources("local", boundary)};
    pooled_run_count = 0;

    blob const big{large_input()};

    // One resolver for the shared uuid; the concurrent leaves differ only in
    // their integer input, so each is a distinct job with a distinct key.
    seri_catalog cat{resources->get_seri_registry()};
    request_uuid const shared_uuid{"pool_registration/concurrent"};
    auto proto{rq_function(leaf_props{shared_uuid}, pooled_concat, big, 0)};
    cat.register_resolver(proto);

    using leaf_req = decltype(proto);
    int const job_count{4};
    std::vector<leaf_req> reqs;
    for (int i = 0; i < job_count; ++i)
    {
        auto req{rq_function(leaf_props{shared_uuid}, pooled_concat, big, i)};
        req.set_pool_name(std::optional<std::string>{"local"});
        reqs.push_back(std::move(req));
    }

    // Concurrent resolution drives multiple in-flight jobs, each observed
    // while its worker records into the shared stores.
    non_caching_request_resolution_context ctx{*resources};
    std::vector<blob> const results{
        cppcoro::sync_wait(resolve_in_parallel(ctx, reqs))};

    // Every job produced the correct, uncorrupted result for its own input.
    REQUIRE(static_cast<int>(results.size()) == job_count);
    for (int i = 0; i < job_count; ++i)
    {
        std::string const expected{
            fmt::format("pooled[{}]:{}", to_string(big), i)};
        REQUIRE(to_string(results[i]) == expected);
    }
    REQUIRE(pooled_run_count == job_count);
}
