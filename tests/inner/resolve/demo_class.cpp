#include <algorithm>
#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/common.h"
#include "../../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/demo_class.h>
#include <cradle/plugins/domain/testing/demo_class_requests.h>

using namespace cradle;

namespace {

static char const tag[] = "[demo_class]";

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

// Tests resolving the two requests related to demo_class
template<caching_level_type caching_level>
void
test_demo_class(std::string const& proxy_name, bool use_shared_memory = false)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};
    ctx.track_blob_file_writers();

    auto req0{rq_make_demo_class<caching_level>(
        3, make_test_blob(ctx, "abc", use_shared_memory))};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req0));
    REQUIRE(res0.get_x() == 3);
    REQUIRE(to_string(res0.get_y()) == "abc");
    auto y0_owner = res0.get_y().mapped_file_data_owner();
    REQUIRE((y0_owner != nullptr) == use_shared_memory);

    auto req1{rq_copy_demo_class<caching_level>(
        demo_class(5, make_test_blob(ctx, "def", use_shared_memory)))};
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req1));
    REQUIRE(res1.get_x() == 5);
    REQUIRE(to_string(res1.get_y()) == "def");
    auto y1_owner = res1.get_y().mapped_file_data_owner();
    REQUIRE((y1_owner != nullptr) == use_shared_memory);
}

} // namespace

TEST_CASE("demo_class - none, local", tag)
{
    test_demo_class<caching_level_type::none>("");
}

TEST_CASE("demo_class - memory, local", tag)
{
    test_demo_class<caching_level_type::memory>("");
}

TEST_CASE("demo_class - full, local", tag)
{
    test_demo_class<caching_level_type::full>("");
}

TEST_CASE("demo_class - none, loopback", tag)
{
    test_demo_class<caching_level_type::none>("loopback");
}

TEST_CASE("demo_class - memory, loopback", tag)
{
    test_demo_class<caching_level_type::memory>("loopback");
}

TEST_CASE("demo_class - full, loopback", tag)
{
    test_demo_class<caching_level_type::full>("loopback");
}

TEST_CASE("demo_class - none, rpclib", tag)
{
    test_demo_class<caching_level_type::none>("rpclib");
}

TEST_CASE("demo_class - memory, rpclib", tag)
{
    test_demo_class<caching_level_type::memory>("rpclib");
}

TEST_CASE("demo_class - full, rpclib", tag)
{
    test_demo_class<caching_level_type::full>("rpclib");
}

TEST_CASE("demo_class - none, local, shmem", tag)
{
    test_demo_class<caching_level_type::none>("", true);
}

TEST_CASE("demo_class - memory, local, shmem", tag)
{
    test_demo_class<caching_level_type::memory>("", true);
}

TEST_CASE("demo_class - full, local, shmem", tag)
{
    test_demo_class<caching_level_type::full>("", true);
}

TEST_CASE("demo_class - none, loopback, shmem", tag)
{
    test_demo_class<caching_level_type::none>("loopback", true);
}

TEST_CASE("demo_class - memory, loopback, shmem", tag)
{
    test_demo_class<caching_level_type::memory>("loopback", true);
}

TEST_CASE("demo_class - full, loopback, shmem", tag)
{
    test_demo_class<caching_level_type::full>("loopback", true);
}

TEST_CASE("demo_class - none, rpclib, shmem", tag)
{
    test_demo_class<caching_level_type::none>("rpclib", true);
}

TEST_CASE("demo_class - memory, rpclib, shmem", tag)
{
    test_demo_class<caching_level_type::memory>("rpclib", true);
}

TEST_CASE("demo_class - full, rpclib, shmem", tag)
{
    test_demo_class<caching_level_type::full>("rpclib", true);
}
