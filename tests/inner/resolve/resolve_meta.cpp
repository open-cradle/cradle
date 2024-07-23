#include <string>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/m0-meta/m0_meta.h"
#include "../../inner-dll/m0-meta/m0_meta_impl.h"
#include "../../support/concurrency_testing.h"
#include "../../support/inner_service.h"
#include "../../support/request.h"
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/utilities/demangle.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/test_dlls_dir.h>

namespace cradle {

namespace {

#define TAG(n) "[inner][resolve][meta][" #n "]"

const containment_data m0_inner_containment{
    request_uuid{m0_inner_uuid},
    get_test_dlls_dir(),
    "test_inner_dll_m0_meta"};

containment_data
make_m0_meta_containment()
{
    request_uuid uuid{m0_meta_p_uuid};
    // Note: uuid in containment data always uncached
    uuid.set_level(caching_level_type::none);
    const containment_data m0_meta_containment{
        std::move(uuid), get_test_dlls_dir(), "test_inner_dll_m0_meta"};
    return m0_meta_containment;
}

// Tests the local resolution of a memory-cached request.
// resolve_local() is bound to that request and locally resolves it.
void
test_resolve_memory_cached(auto const& resolve_local)
{
    std::string proxy_name{""};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};
    auto& cache{resources->memory_cache()};

    resolve_local(ctx);
    auto info0 = get_summary_info(cache);
    CHECK(info0.hit_count == 0);
    CHECK(info0.miss_count == 1);

    resolve_local(ctx);
    auto info1 = get_summary_info(cache);
    CHECK(info1.hit_count == 1);
    CHECK(info1.miss_count == 1);
}

// Tests the local resolution of a fully-cached request.
// resolve_local() is bound to that request and locally resolves it.
void
test_resolve_fully_cached(auto const& resolve_local)
{
    std::string proxy_name{""};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};
    auto& mem_cache{resources->memory_cache()};
    auto& disk_cache{
        static_cast<local_disk_cache&>(resources->secondary_cache())};
    seri_catalog cat{resources->get_seri_registry()};
    // To deserialize the inner request(s) read from the disk cache
    cat.register_resolver(m0_make_inner_request_func(0, 0));

    resolve_local(ctx);
    auto info0{disk_cache.get_summary_info()};
    CHECK(info0.hit_count == 0);
    CHECK(info0.miss_count == 1);

    clear_unused_entries(mem_cache);
    sync_wait_write_disk_cache(*resources);

    resolve_local(ctx);
    auto info1{disk_cache.get_summary_info()};
    CHECK(info1.hit_count == 1);
    CHECK(info1.miss_count == 1);
}

void
test_resolve_meta(
    testing_request_context& ctx,
    auto const& meta_req,
    containment_data const* inner_containment = nullptr)
{
    // Resolve the meta request to a "normal" inner request
    auto inner_req = cppcoro::sync_wait(resolve_request(ctx, meta_req));
    static_assert(Request<decltype(inner_req)>);

    // Resolve the inner request to a value
    if (inner_containment)
    {
        inner_req.set_containment(*inner_containment);
    }
    auto res = cppcoro::sync_wait(resolve_request(ctx, inner_req));

    REQUIRE(res == 3 + 2);
}

void
test_resolve_meta_with_setup(
    auto const& meta_req,
    std::string const& proxy_name,
    containment_data const* inner_containment = nullptr)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};

    seri_catalog cat{resources->get_seri_registry()};
    if (!proxy_name.empty())
    {
        // To deserialize the request received from the remote
        cat.register_resolver(m0_make_inner_request_func(0, 0));

        auto& proxy{resources->get_proxy(proxy_name)};
        proxy.load_shared_library(
            get_test_dlls_dir(), "test_inner_dll_m0_meta");
    }

    test_resolve_meta(ctx, meta_req, inner_containment);
}

} // namespace

TEST_CASE("resolve meta - local", TAG(0))
{
    auto meta_req{rq_test_m0_metap_impl(3, 2)};

    test_resolve_meta_with_setup(meta_req, "");
}

TEST_CASE("resolve meta - loopback", TAG(1))
{
    auto meta_req{rq_test_m0_metap(3, 2)};

    test_resolve_meta_with_setup(meta_req, "loopback");
}

TEST_CASE("resolve meta - rpclib", TAG(2))
{
    auto meta_req{rq_test_m0_metap(3, 2)};

    test_resolve_meta_with_setup(meta_req, "rpclib");
}

TEST_CASE("resolve meta - rpclib, contained", TAG(3))
{
    auto meta_containment{make_m0_meta_containment()};
    auto meta_req{rq_test_m0_metap(&meta_containment, 3, 2)};

    test_resolve_meta_with_setup(meta_req, "rpclib", &m0_inner_containment);
}

TEST_CASE("resolve meta - rpclib, normalized", TAG(4))
{
    auto meta_req{
        rq_test_m0_metan(normalize_arg<int, m0_proxy_props_type>(3), 2)};

    test_resolve_meta_with_setup(meta_req, "rpclib");
}

TEST_CASE("resolve meta - rpclib, normalized, contained", TAG(5))
{
    auto meta_containment{make_m0_meta_containment()};
    auto meta_req{rq_test_m0_metan(
        &meta_containment, normalize_arg<int, m0_proxy_props_type>(3), 2)};

    test_resolve_meta_with_setup(meta_req, "rpclib", &m0_inner_containment);
}

TEST_CASE("resolve meta - memory cached", TAG(6))
{
    auto meta_req{rq_test_m0_metap_impl<caching_level_type::memory>(3, 2)};

    test_resolve_memory_cached([&meta_req](testing_request_context& ctx) {
        test_resolve_meta(ctx, meta_req);
    });
}

TEST_CASE("resolve meta - fully cached", TAG(7))
{
    auto meta_req{rq_test_m0_metap_impl<caching_level_type::full>(3, 2)};

    test_resolve_fully_cached([&meta_req](testing_request_context& ctx) {
        test_resolve_meta(ctx, meta_req);
    });
}

namespace {

// A metavec request is resolved to a vector of requests.

auto
make_metavec_input()
{
    return std::vector<int>{1, 2, 3, 4, 5, 6};
}

void
test_resolve_metavec(testing_request_context& ctx, auto const& metavec_req)
{
    // Resolve metavec request to a vector of "normal" inner requests
    auto vec_inner_req = cppcoro::sync_wait(resolve_request(ctx, metavec_req));

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, vec_inner_req));

    std::vector<int> expected{1 + 2, 3 + 4, 5 + 6};
    REQUIRE(res == expected);
}

void
test_resolve_metavec_with_setup(
    auto const& metavec_req, std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};

    seri_catalog cat{resources->get_seri_registry()};
    if (!proxy_name.empty())
    {
        // To deserialize the requests received from the remote
        cat.register_resolver(m0_make_inner_request_func(0, 0));

        auto& proxy{resources->get_proxy(proxy_name)};
        proxy.load_shared_library(
            get_test_dlls_dir(), "test_inner_dll_m0_meta");
    }

    test_resolve_metavec(ctx, metavec_req);
}

} // namespace

TEST_CASE("resolve metavec - local", TAG(10))
{
    auto metavec_req{rq_test_m0_metavecp_impl(make_metavec_input())};

    test_resolve_metavec_with_setup(metavec_req, "");
}

TEST_CASE("resolve metavec - loopback", TAG(11))
{
    auto metavec_req{rq_test_m0_metavecp(make_metavec_input())};

    test_resolve_metavec_with_setup(metavec_req, "loopback");
}

TEST_CASE("resolve metavec - rpclib", TAG(12))
{
    auto metavec_req{rq_test_m0_metavecp(make_metavec_input())};
    test_resolve_metavec_with_setup(metavec_req, "rpclib");
}

TEST_CASE("resolve metavec - memory cached", TAG(13))
{
    auto metavec_req{rq_test_m0_metavecp_impl<caching_level_type::memory>(
        make_metavec_input())};

    test_resolve_memory_cached([&metavec_req](testing_request_context& ctx) {
        test_resolve_metavec(ctx, metavec_req);
    });
}

TEST_CASE("resolve metavec - fully cached", TAG(14))
{
    auto metavec_req{rq_test_m0_metavecp_impl<caching_level_type::full>(
        make_metavec_input())};

    test_resolve_fully_cached([&metavec_req](testing_request_context& ctx) {
        test_resolve_metavec(ctx, metavec_req);
    });
}

} // namespace cradle
