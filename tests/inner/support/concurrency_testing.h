#ifndef CRADLE_TESTS_INNER_SUPPORT_CONCURRENCY_TESTING_H
#define CRADLE_TESTS_INNER_SUPPORT_CONCURRENCY_TESTING_H

#include <chrono>
#include <thread>
#include <utility>

#include <catch2/catch.hpp>

#include <cradle/inner/service/internals.h>

namespace cradle {

struct inner_service_core;

// Wait up to a second to see if a condition occurs (i.e., returns true).
// Check once per millisecond to see if it occurs.
// Return whether or not it occurs.
// A second argument can be supplied to change the time to wait.
template<class Condition>
bool
occurs_soon(Condition&& condition, int wait_time_in_ms = 1000)
{
    int n = 0;
    while (true)
    {
        if (std::forward<Condition>(condition)())
            return true;
        if (++n > wait_time_in_ms)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// Data is written to the disk cache in a background thread;
// wait until all these write operations have completed.
inline void
sync_wait_write_disk_cache(inner_service_core& service)
{
    REQUIRE(occurs_soon([&] {
        return service.inner_internals().disk_write_pool.get_tasks_total()
               == 0;
    }));
}

} // namespace cradle

#endif
