#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>

#include "../inner-dll/v1/adder_v1.h"
#include "../inner-dll/v1/adder_v1_impl.h"
#include "../support/inner_service.h"
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

static const containment_data v1_containment{
    request_uuid{adder_v1p_uuid}, get_test_dlls_dir(), "test_inner_dll_v1"};

void
BM_Resolve(benchmark::State& state, containment_data const* containment)
{
    std::string proxy_name{"rpclib"};
    auto req{rq_test_adder_v1p_impl(containment, 7, 2)};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");
    testing_request_context ctx{*resources, proxy_name};
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            cppcoro::sync_wait(resolve_request(ctx, req)));
    }
}

void
BM_ResolveUncontained(benchmark::State& state)
{
    BM_Resolve(state, nullptr);
}

void
BM_ResolveContained(benchmark::State& state)
{
    BM_Resolve(state, &v1_containment);
}

BENCHMARK(BM_ResolveUncontained);
BENCHMARK(BM_ResolveContained);
