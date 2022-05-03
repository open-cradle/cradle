#include <cradle/inner/generic/addition.h>

namespace cradle {

cppcoro::task<int>
addition(addition_request const& req)
{
    int res = 0;
    for (auto const& subreq : req.get_subrequests())
    {
        res += subreq->get_literal();
    }
    co_return res;
}

cppcoro::task<int>
addition_request::create_task() const
{
    return addition(*this);
}

} // namespace cradle
