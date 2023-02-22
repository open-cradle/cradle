#ifndef CRADLE_THINKNODE_DISK_CACHED_H
#define CRADLE_THINKNODE_DISK_CACHED_H

#include <functional>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// Resolves an "old-style" request (a fully_cached() call), using the disk
// cache provided by the given resources, and some sort of serialization.
// CRADLE inner only declares this template function; a plugin should
// provide its definition.
// This could be a coroutine.
template<typename Value>
cppcoro::task<Value>
disk_cached(
    inner_resources& resources,
    captured_id key,
    std::function<cppcoro::task<Value>()> create_task);

} // namespace cradle

#endif
