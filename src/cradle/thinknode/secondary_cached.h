#ifndef CRADLE_THINKNODE_SECONDARY_CACHED_H
#define CRADLE_THINKNODE_SECONDARY_CACHED_H

#include <functional>
#include <utility>

#include <cppcoro/fmap.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/functional.h>
#include <cradle/typing/core/dynamic.h>
#include <cradle/typing/core/type_definitions.h>

namespace cradle {

// Resolves an "old-style" request (a fully_cached() call), using the secondary
// cache provided by the given resources, and serializing via a native
// encoding.
// These functions could be coroutines.

// The default (for a value that is not a blob or a dynamic) is to convert the
// value to a dynamic and serialize that.
template<typename Value>
cppcoro::task<Value>
secondary_cached(
    inner_resources& resources,
    captured_id key,
    std::function<cppcoro::task<Value>()> create_task)
{
    return cppcoro::make_task(cppcoro::fmap(
        CRADLE_LAMBDIFY(from_dynamic<Value>),
        secondary_cached<dynamic>(
            resources, key, [create_task = std::move(create_task)]() {
                return cppcoro::make_task(cppcoro::fmap(
                    CRADLE_LAMBDIFY(to_dynamic<Value>), create_task()));
            })));
}

// There is no need to convert a dynamic to a dynamic.
template<>
cppcoro::task<dynamic>
secondary_cached(
    inner_resources& resources,
    captured_id key,
    std::function<cppcoro::task<dynamic>()> create_task);

// A blob will be stored as-is (no serialization needed).
template<>
cppcoro::task<blob>
secondary_cached(
    inner_resources& core,
    captured_id key,
    std::function<cppcoro::task<blob>()> create_task);

} // namespace cradle

#endif
