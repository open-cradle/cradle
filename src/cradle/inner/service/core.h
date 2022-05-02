#ifndef CRADLE_INNER_SERVICE_CORE_H
#define CRADLE_INNER_SERVICE_CORE_H

#include <optional>

#include <cppcoro/task.hpp>

#include <cradle/inner/caching/disk_cache.h>
#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/generic/generic.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/internals.h>

namespace cradle {

struct inner_service_config
{
    // config for the immutable memory cache
    std::optional<immutable_cache_config> immutable_cache;

    // config for the disk cache
    std::optional<disk_cache_config> disk_cache;
};

struct inner_service_core
{
    void
    inner_reset();
    void
    inner_reset(inner_service_config const& config);

    detail::inner_service_core_internals&
    inner_internals()
    {
        return *impl_;
    }

 private:
    std::unique_ptr<detail::inner_service_core_internals> impl_;
};

template<typename Value>
cppcoro::task<Value>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<Value>()> create_task);

// The inner core has just this one specialization.
template<>
cppcoro::task<blob>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<blob>()> create_task);

template<typename Request>
cppcoro::task<typename Request::value_type>
new_disk_cached(
    inner_service_core& core, std::shared_ptr<Request> const& shared_req);

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
cached(
    inner_service_core& core, captured_id const& key, TaskCreator task_creator)
{
    immutable_cache_ptr<Value> ptr(
        core.inner_internals().cache, key, task_creator);
    return ptr.task();
}

template<typename Request>
cppcoro::shared_task<typename Request::value_type>
memory_cached(inner_service_core& core, std::shared_ptr<Request> const& req)
{
    using Value = typename Request::value_type;
    immutable_cache_ptr<Value> ptr(core.inner_internals().cache, req);
    return ptr.task();
}

template<typename Request>
cppcoro::shared_task<typename Request::value_type>
memory_cached(
    inner_service_core& core, std::shared_ptr<Request> const& req, int)
{
    using Value = typename Request::value_type;
    auto disk_fallback = [&core, req]() -> cppcoro::task<Value> {
        return new_disk_cached(core, req);
    };
    immutable_cache_ptr<Value> ptr(
        core.inner_internals().cache, req, disk_fallback);
    return ptr.task();
}

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
fully_cached(
    inner_service_core& core, captured_id const& key, TaskCreator task_creator)
{
    // cached() will ensure that a captured id_interface object exists
    // equalling `key`; it will pass a reference to that object to the lambda.
    // It may be a different object from `key`: `key` may no longer exist when
    // the lambda is called.
    return cached<Value>(
        core, key, [&core, task_creator](id_interface const& key1) {
            return disk_cached<Value>(core, key1, std::move(task_creator));
        });
}

template<typename Request>
cppcoro::shared_task<typename Request::value_type>
eval_uncached(Request const& req)
{
    co_return co_await req.create_task();
}

template<typename Request>
cppcoro::shared_task<typename Request::value_type>
new_fully_cached(
    inner_service_core& core, std::shared_ptr<Request> const& shared_req)
{
    Request const& req = *shared_req;
    if constexpr (req.get_caching_level() == caching_level_type::none)
    {
        return eval_uncached(req);
    }
    else if constexpr (req.get_caching_level() == caching_level_type::memory)
    {
        return memory_cached(core, shared_req);
    }
    else
    {
        return memory_cached(core, shared_req, 0);
    }
}

} // namespace cradle

#endif
