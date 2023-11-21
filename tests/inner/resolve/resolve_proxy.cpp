#include <stdexcept>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][proxy]";

} // namespace

TEST_CASE("evaluate proxy request, plain args", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1p(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no entry found for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res == expected);
}

TEST_CASE("evaluate proxy request, normalized args", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1n(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no entry found for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res == expected);
}

TEST_CASE("attempt to resolve proxy request locally", tag)
{
    std::string proxy_name{""};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    auto req{rq_test_adder_v1p(7, 2)};

    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};

    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_request(ctx, req)), std::logic_error);
}

// Tracks the progress of all "busy" requests (below).
class busy_progress
{
 public:
    void
    handle_error(int attempt, std::exception const& e)
    {
        std::scoped_lock lock{mutex_};
        if (!error_occurred_)
        {
            error_message_
                = fmt::format("attempt {}: caught {}", attempt, e.what());
            error_occurred_ = true;
        }
    }

    bool
    error_occurred() const
    {
        return error_occurred_;
    }

    std::string const&
    error_message() const
    {
        std::scoped_lock lock{mutex_};
        return error_message_;
    }

 private:
    mutable std::mutex mutex_;
    std::atomic<bool> error_occurred_{false};
    std::string error_message_;
};

// Resolves a "busy" request.
auto
resolve_busy_request(testing_request_context& ctx, int delay_millis)
{
    int const loops = 1;
    int const expected{loops + delay_millis};
    constexpr auto level{caching_level_type::none};
    auto req{rq_non_cancellable_func<level>(loops, delay_millis)};
    ResolutionConstraintsRemoteSync constraints;
    auto actual = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    return std::make_pair(actual, expected);
}

// Thread function resolving a "busy" request.
static void
busy_thread_func(
    testing_request_context& ctx, int attempt, busy_progress& progress)
{
    try
    {
        auto [actual, expected] = resolve_busy_request(ctx, 200);
        if (actual != expected)
        {
            throw std::runtime_error(fmt::format("actual {}", actual));
        }
    }
    catch (std::exception const& e)
    {
        progress.handle_error(attempt, e);
    }
}

// When too many rpclib server handler threads are busy, a following
// resolve_sync request should immediately fail.
TEST_CASE("rpclib server busy on many parallel resolve_sync requests", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};

    // Send lots of resolve_sync requests to the server, until it starts
    // responding with "busy" errors.
    constexpr int max_attempts{80};
    std::vector<std::jthread> threads;
    busy_progress progress;
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        threads.emplace_back(
            busy_thread_func, std::ref(ctx), attempt, std::ref(progress));
        if ((attempt + 1) % 8 == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (progress.error_occurred())
        {
            break;
        }
    }
    REQUIRE(progress.error_occurred());
    REQUIRE(
        progress.error_message().find(
            "all threads for this request type are busy")
        != std::string::npos);

    // Wait until at least one server thread has become idle again.
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // The server should now accept new resolve_sync requests.
    auto [actual, expected] = resolve_busy_request(ctx, 1);
    REQUIRE(actual == expected);
}
