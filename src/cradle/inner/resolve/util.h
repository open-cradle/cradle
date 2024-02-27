#ifndef CRADLE_INNER_RESOLVE_UTIL_H
#define CRADLE_INNER_RESOLVE_UTIL_H

#include <string>

#include <cppcoro/task.hpp>

namespace cradle {

class introspective_context;
class tasklet_tracker;

// Introspection for a "co_await shared_task" call that is part of a
// resolve_request()
class coawait_introspection
{
 public:
    coawait_introspection(
        introspective_context_intf& ctx,
        std::string const& pool_name,
        std::string const& title);

    ~coawait_introspection();

 private:
    introspective_context_intf& ctx_;
    tasklet_tracker* tasklet_;
};

inline cppcoro::task<void>
dummy_coroutine()
{
    co_return;
}

cppcoro::task<serialized_result>
resolve_serialized_introspective(
    introspective_context_intf& ctx,
    std::string proxy_name,
    std::string title,
    std::string seri_req);

} // namespace cradle

#endif
