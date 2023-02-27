#ifndef CRADLE_INNER_UTILITIES_FOR_ASYNC_H
#define CRADLE_INNER_UTILITIES_FOR_ASYNC_H

#include <utility>

#include <cppcoro/task.hpp>

namespace cradle {

template<class Sequence, class Function>
cppcoro::task<>
for_async(Sequence sequence, Function&& function)
{
    auto i = co_await sequence.begin();
    auto const end = sequence.end();
    while (i != end)
    {
        std::forward<Function>(function)(*i);
        (void) co_await ++i;
    }
}

} // namespace cradle

#endif
