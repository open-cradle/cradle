#ifndef CRADLE_TESTS_SUPPORT_CONCURRENCY_TESTING_H
#define CRADLE_TESTS_SUPPORT_CONCURRENCY_TESTING_H

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <cradle/inner/service/resources.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>

namespace cradle {

struct assertion_error : public std::logic_error
{
    assertion_error() : std::logic_error("Assertion failed")
    {
    }

    assertion_error(std::string const& what) : std::logic_error(what)
    {
    }

    assertion_error(char const* what) : std::logic_error(what)
    {
    }
};

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
sync_wait_write_disk_cache(inner_resources& resources)
{
    auto& disk_cache{
        static_cast<local_disk_cache&>(resources.secondary_cache())};

    if (!occurs_soon([&] { return !disk_cache.busy_writing_to_file(); }))
    {
        throw assertion_error("Disk cache writes not finished in time");
    }
}

} // namespace cradle

#endif
