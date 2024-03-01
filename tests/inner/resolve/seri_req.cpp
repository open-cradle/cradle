#include <algorithm>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../support/inner_service.h"
#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/introspection/tasklet_util.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/testing_seri_catalog.h>
#include <cradle/plugins/serialization/response/msgpack.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

static char const tag[] = "[inner][resolve][seri_req]";

static void
test_resolve(std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    testing_request_context ctx{*resources, nullptr, proxy_name};

    constexpr auto caching_level{caching_level_type::full};
    auto req{rq_make_some_blob<caching_level>(256, false)};
    std::string seri_req{serialize_request(req)};
    auto seri_resp
        = cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req));
    blob response = deserialize_response<blob>(seri_resp.value());
    seri_resp.on_deserialized();

    REQUIRE(response.size() == 256);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
}

TEST_CASE("resolve serialized request, locally", tag)
{
    test_resolve("");
}

TEST_CASE("resolve serialized request, loopback", tag)
{
    test_resolve("loopback");
}

TEST_CASE("resolve serialized request, rpclib", tag)
{
    test_resolve("rpclib");
}

TEST_CASE("resolve serialized request, DLL", tag)
{
    std::string proxy_name{""};
    auto resources{make_inner_test_resources(proxy_name, no_domain_option())};
    non_caching_request_resolution_context ctx{*resources};

    auto req{rq_test_adder_v1p(7, 2)};
    int expected{7 + 2};
    std::string seri_req{serialize_request(req)};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req)),
        Catch::Contains("no entry found for uuid"));

    std::string dll_name{"test_inner_dll_v1"};
    auto& the_dlls{resources->the_dlls()};
    the_dlls.load(get_test_dlls_dir(), dll_name);

    auto seri_resp
        = cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req));
    int response = deserialize_response<int>(seri_resp.value());
    seri_resp.on_deserialized();

    REQUIRE(response == expected);

    the_dlls.unload(dll_name);

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req)),
        Catch::Contains("no entry found for uuid"));
}

// TODO consider creating a remote_proxy implementation for local operation.
static void
clear_unused_mem_cache_entries(context_intf& ctx)
{
    if (auto* rctx = cast_ctx_to_ptr<remote_context_intf>(ctx))
    {
        auto& proxy{rctx->get_proxy()};
        proxy.clear_unused_mem_cache_entries();
    }
    else
    {
        clear_unused_entries(ctx.get_resources().memory_cache());
    }
}

// Resolves a serialized request, storing the result in a blob file.
// Returns the path to that blob file.
static std::string
resolve_make_some_blob_file_seri(
    context_intf& ctx,
    std::string const& seri_req,
    cache_record_lock* lock_ptr)
{
    auto seri_resp = cppcoro::sync_wait(resolve_serialized_request(
        ctx,
        seri_req,
        seri_cache_record_lock_t{lock_ptr, remote_cache_record_id{}}));
    blob resp = deserialize_response<blob>(seri_resp.value());
    seri_resp.on_deserialized();
    REQUIRE(resp.size() == 256);
    REQUIRE(resp.data()[0xff] == static_cast<std::byte>(0x55));
    auto* resp_owner = resp.mapped_file_data_owner();
    REQUIRE(resp_owner != nullptr);
    return resp_owner->mapped_file();
}

template<template<typename Req> class CtxMaker>
static void
test_resolve_with_lock(std::string const& proxy_name, bool introspective)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    remote_proxy* proxy{nullptr};
    if (!proxy_name.empty())
    {
        proxy = &resources->get_proxy(proxy_name);
    }

    constexpr auto caching_level{caching_level_type::memory};
    auto req{rq_make_some_blob<caching_level>(256, true)};
    std::string seri_req{serialize_request(req)};
    auto lock_ptr{std::make_unique<cache_record_lock>()};

    // Resolve the serialized request, obtaining a memory cache lock.
    CtxMaker<decltype(req)> ctx_maker{*resources, proxy_name, introspective};
    auto ctx0 = ctx_maker(req);
    auto file0 = resolve_make_some_blob_file_seri(*ctx0, seri_req, &*lock_ptr);

    clear_unused_mem_cache_entries(*ctx0);
    // The memory cache should still hold the entry referring to file0, so
    // re-resolving the request should return the same blob file.
    auto ctx1 = ctx_maker(req);
    auto file1 = resolve_make_some_blob_file_seri(*ctx1, seri_req, nullptr);
    CHECK(file1 == file0);

    lock_ptr.reset();
    clear_unused_mem_cache_entries(*ctx1);
    // The memory cache no longer refers to file0; re-resolving the request
    // will create a new blob file.
    auto ctx2 = ctx_maker(req);
    auto file2 = resolve_make_some_blob_file_seri(*ctx2, seri_req, nullptr);
    CHECK(file2 != file0);

    // TODO make this work for rpclib; need some kind of session for
    // resolve_sync() and get_tasklet_infos()
    if (introspective && proxy_name != "rpclib")
    {
        tasklet_info_list infos;
        if (!proxy)
        {
            auto& admin{resources->the_tasklet_admin()};
            infos = get_tasklet_infos(admin, true);
        }
        else
        {
            infos = proxy->get_tasklet_infos(true);
        }
#if 0
        fmt::print("tasklet_infos for {}\n", proxy_name);
        dump_tasklet_infos(infos);
#endif
        tasklet_info_list resolve_infos;
        std::copy_if(
            infos.begin(),
            infos.end(),
            std::back_inserter(resolve_infos),
            [](tasklet_info const& info) {
                return info.pool_name() == "resolve_request";
            });
        // The request was resolved 3 times.
        CHECK(resolve_infos.size() == 3);
        for (auto const& info : resolve_infos)
        {
            CHECK(info.have_client());
            auto const& events{info.events()};
            CHECK(!events.empty());
            auto const& last_event = events.back();
            CHECK(last_event.what() == tasklet_event_type::FINISHED);
        }
    }
}

// Creates a context for synchronously resolving Req objects
template<typename Req>
class SyncCtxMaker
{
 public:
    SyncCtxMaker(
        inner_resources& resources,
        std::string const& proxy_name,
        bool introspective)
        : resources_{resources},
          proxy_name_{proxy_name},
          introspective_{introspective}
    {
    }

    auto
    operator()(Req const& req)
    {
        // Sync: context does not depend on the request; use the same context
        // for all requests
        if (!ctx_)
        {
            tasklet_tracker* tasklet{};
            if (introspective_)
            {
                auto& admin{resources_.the_tasklet_admin()};
                introspection_set_capturing_enabled(admin, true);
                introspection_set_logging_enabled(admin, true);
                tasklet
                    = create_tasklet_tracker(admin, "test", "make_some_blob");
            }
            ctx_ = std::make_shared<testing_request_context>(
                resources_, tasklet, proxy_name_);
        }
        return ctx_;
    }

 private:
    inner_resources& resources_;
    std::string proxy_name_;
    bool introspective_;
    std::shared_ptr<testing_request_context> ctx_;
};

static void
test_resolve_with_lock_sync(std::string const& proxy_name, bool introspective)
{
    return test_resolve_with_lock<SyncCtxMaker>(proxy_name, introspective);
}

TEST_CASE("resolve serialized request with lock, sync, locally", tag)
{
    test_resolve_with_lock_sync("", false);
}

TEST_CASE("resolve serialized request with lock, sync, loopback", tag)
{
    test_resolve_with_lock_sync("loopback", false);
}

TEST_CASE("resolve serialized request with lock, sync, rpclib", tag)
{
    test_resolve_with_lock_sync("rpclib", false);
}

TEST_CASE("resolve seri request with lock; sync, intrsp, locally", tag)
{
    test_resolve_with_lock_sync("", true);
}

TEST_CASE("resolve seri request with lock; sync, intrsp, loopback", tag)
{
    test_resolve_with_lock_sync("loopback", true);
}

TEST_CASE("resolve seri request with lock; sync, intrsp, rpclib", tag)
{
    test_resolve_with_lock_sync("rpclib", true);
}

// Creates a context for locally/asynchronously resolving Req objects
template<typename Req>
class LocalAsyncCtxMaker
{
 public:
    LocalAsyncCtxMaker(
        inner_resources& resources,
        std::string const& proxy_name,
        bool introspective)
        : resources_{resources}, introspective_{introspective}
    {
    }

    auto
    operator()(Req const& req)
    {
        // Async: use a fresh context for each request
        auto tree_ctx = std::make_shared<local_atst_tree_context>(resources_);
        // TODO Populating ctx tree here is wrong because
        // seri_resolver_impl::resolve() also does that
        // return make_local_async_ctx_tree(tree_ctx, req);
        auto ctx
            = std::make_shared<local_atst_context>(tree_ctx, nullptr, true);
        if (introspective_)
        {
            auto& admin{resources_.the_tasklet_admin()};
            introspection_set_capturing_enabled(admin, true);
            introspection_set_logging_enabled(admin, true);
            auto tasklet
                = create_tasklet_tracker(admin, "test", "make_some_blob");
            ctx->push_tasklet(*tasklet);
        }
        return ctx;
    }

 private:
    inner_resources& resources_;
    bool introspective_;
};

// Creates a context for remotely/asynchronously resolving Req objects
// TODO try to unify RemoteAsyncCtxMaker and LocalAsyncCtxMaker
template<typename Req>
class RemoteAsyncCtxMaker
{
 public:
    RemoteAsyncCtxMaker(
        inner_resources& resources,
        std::string const& proxy_name,
        bool introspective)
        : resources_{resources},
          proxy_name_{proxy_name},
          introspective_{introspective}
    {
    }

    auto
    operator()(Req const& req)
    {
        // Async: use a fresh context for each request
        auto tree_ctx = std::make_shared<proxy_atst_tree_context>(
            resources_, proxy_name_);
        auto ctx = std::make_shared<root_proxy_atst_context>(tree_ctx);
        if (introspective_)
        {
            auto& admin{resources_.the_tasklet_admin()};
            introspection_set_capturing_enabled(admin, true);
            introspection_set_logging_enabled(admin, true);
            auto tasklet
                = create_tasklet_tracker(admin, "test", "make_some_blob");
            ctx->push_tasklet(*tasklet);
        }
        return ctx;
    }

 private:
    inner_resources& resources_;
    std::string proxy_name_;
    bool introspective_;
};

TEST_CASE("resolve serialized request with lock, async, locally", tag)
{
    return test_resolve_with_lock<LocalAsyncCtxMaker>("", false);
}

TEST_CASE("resolve serialized request with lock, async, loopback", tag)
{
    return test_resolve_with_lock<RemoteAsyncCtxMaker>("loopback", false);
}

TEST_CASE("resolve serialized request with lock, async, rpclib", tag)
{
    return test_resolve_with_lock<RemoteAsyncCtxMaker>("rpclib", false);
}

TEST_CASE("resolve serialized request with lock; async, intrsp, locally", tag)
{
    return test_resolve_with_lock<LocalAsyncCtxMaker>("", true);
}

TEST_CASE("resolve serialized request with lock; async, intrsp, loopback", tag)
{
    return test_resolve_with_lock<RemoteAsyncCtxMaker>("loopback", true);
}

TEST_CASE("resolve serialized request with lock; async, intrsp, rpclib", tag)
{
    return test_resolve_with_lock<RemoteAsyncCtxMaker>("rpclib", true);
}
