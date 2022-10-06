#ifndef CRADLE_INNER_SERVICE_REQUEST_H
#define CRADLE_INNER_SERVICE_REQUEST_H

#include <sstream>
#include <type_traits>

#include <cereal/archives/binary.hpp>
#include <cppcoro/fmap.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/caching/disk_cache.h>
#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/internals.h>
#include <cradle/inner/service/types.h>

namespace cradle {

template<typename Ctx, typename Req>
    requires(Req::caching_level == caching_level_type::memory)
cppcoro::task<typename Req::value_type> resolve_disk_cached(
    Ctx& ctx, Req const& req)
{
    return req.resolve(ctx);
}

// Values are stored in the disk cache after serializing using cereal.
// Supporting different serialization algorithms won't be easy.
template<typename Value>
blob
serialize_value(Value const& value)
{
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(value);
    return make_blob(ss.str());
}

template<typename Value>
Value
deserialize_value(blob const& x)
{
    auto data = reinterpret_cast<char const*>(x.data());
    std::stringstream is;
    is.str(std::string(data, x.size()));
    cereal::BinaryInputArchive iarchive(is);
    Value res;
    iarchive(res);
    return res;
}

template<typename Ctx, typename Req>
    requires(FullyCachedContextRequest<Ctx, Req>)
cppcoro::task<typename Req::value_type> resolve_disk_cached(
    Ctx& ctx, Req const& req)
{
    using Value = typename Req::value_type;
    inner_service_core& core{ctx.get_service()};
    captured_id const& key{req.get_captured_id()};
    auto create_blob_task = [&]() -> cppcoro::task<blob> {
        co_return serialize_value(co_await req.resolve(ctx));
    };
    co_return deserialize_value<Value>(
        co_await disk_cached<blob>(core, key, create_blob_task));
}

// Subsuming the concepts from the previous template
template<typename Ctx, typename Req>
    requires(
        FullyCachedContextRequest<Ctx, Req>
        && std::same_as<typename Req::value_type, blob>)
cppcoro::task<blob> resolve_disk_cached(Ctx& ctx, Req const& req)
{
    inner_service_core& core{ctx.get_service()};
    captured_id const& key{req.get_captured_id()};
    auto create_blob_task = [&]() -> cppcoro::task<blob> {
        co_return co_await req.resolve(ctx);
    };
    return disk_cached<blob>(core, key, create_blob_task);
}

// This function, being a coroutine, takes key by value.
// The caller should ensure that cache, ctx and req outlive the coroutine.
template<typename Ctx, typename Req>
    requires CachedContextRequest<Ctx, Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_on_memory_cache_miss(
    detail::immutable_cache_impl& cache,
    captured_id key,
    Ctx& ctx,
    Req const& req)
{
    // cache and key could be retrieved from ctx and req, respectively.
    try
    {
        auto value = co_await resolve_disk_cached(ctx, req);
        record_immutable_cache_value(cache, *key, deep_sizeof(value));
        co_return value;
    }
    catch (...)
    {
        record_immutable_cache_failure(cache, *key);
        throw;
    }
}

template<typename Ctx, typename Req>
    requires IntrospectiveContextRequest<Ctx, Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_introspected(
    Ctx& ctx,
    Req const& req,
    cppcoro::shared_task<typename Req::value_type> shared_task,
    tasklet_tracker& client)
{
    client.on_before_await(
        req.get_introspection_title(), *req.get_captured_id());
    auto res = co_await shared_task;
    client.on_after_await();
    co_return res;
}

// Returns a reference to the shared_task stored in the memory cache.
// Not copying shared_task's looks like a worthwhile optimization.
template<typename Ctx, typename Req>
    requires(CachedContextRequest<Ctx, Req> && !Req::introspective)
cppcoro::shared_task<typename Req::value_type> const& resolve_request_cached(
    Ctx& ctx, Req const& req)
{
    immutable_cache_ptr<typename Req::value_type> ptr{
        ctx.get_cache(),
        req.get_captured_id(),
        [&](detail::immutable_cache_impl& internal_cache,
            captured_id const& key) {
            return resolve_request_on_memory_cache_miss<Ctx, Req>(
                internal_cache, key, ctx, req);
        }};
    return ptr.task();
}

// Returns the shared_task by value.
template<typename Ctx, typename Req>
    requires(CachedIntrospectiveContextRequest<Ctx, Req>)
cppcoro::shared_task<typename Req::value_type> resolve_request_cached(
    Ctx& ctx, Req const& req)
{
    immutable_cache_ptr<typename Req::value_type> ptr{
        ctx.get_cache(),
        req.get_captured_id(),
        [&](detail::immutable_cache_impl& internal_cache,
            captured_id const& key) {
            return resolve_request_on_memory_cache_miss<Ctx, Req>(
                internal_cache, key, ctx, req);
        }};
    auto shared_task = ptr.task();
    if (auto tasklet = ctx.get_tasklet())
    {
        return resolve_request_introspected<Ctx, Req>(
            ctx, req, shared_task, *tasklet);
    }
    return shared_task;
}

// If second arg is a request, one of the following templates will subsume
template<typename Ctx, typename Val>
cppcoro::task<Val>
resolve_request(Ctx& ctx, Val const& val)
{
    co_return val;
}

template<typename Ctx, typename Req>
    requires UncachedContextRequest<Ctx, Req>
auto
resolve_request(Ctx& ctx, Req const& req)
{
    return req.resolve(ctx);
}

template<typename Ctx, typename Req>
    requires(CachedContextRequest<Ctx, Req> && !Req::introspective)
cppcoro::shared_task<typename Req::value_type> const& resolve_request(
    Ctx& ctx, Req const& req)
{
    return resolve_request_cached(ctx, req);
}

template<typename Ctx, typename Req>
    requires(CachedContextRequest<Ctx, Req> && Req::introspective)
cppcoro::shared_task<typename Req::value_type> resolve_request(
    Ctx& ctx, Req const& req)
{
    return resolve_request_cached(ctx, req);
}

template<typename Ctx, typename Req>
    requires UncachedContextRequest<Ctx, Req>
auto
resolve_request(Ctx& ctx, std::unique_ptr<Req> const& req)
{
    return req->resolve(ctx);
}

template<typename Ctx, typename Req>
    requires CachedContextRequest<Ctx, Req>
auto
resolve_request(Ctx& ctx, std::unique_ptr<Req> const& req)
{
    return resolve_request_cached(ctx, *req);
}

template<typename Ctx, typename Req>
    requires UncachedContextRequest<Ctx, Req>
auto
resolve_request(Ctx& ctx, std::shared_ptr<Req> const& req)
{
    return req->resolve(ctx);
}

template<typename Ctx, typename Req>
    requires CachedContextRequest<Ctx, Req>
auto
resolve_request(Ctx& ctx, std::shared_ptr<Req> const& req)
{
    return resolve_request_cached(ctx, *req);
}

} // namespace cradle

#endif
