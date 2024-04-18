#include <cradle/plugins/domain/testing/demo_class_requests.h>

namespace cradle {

cppcoro::task<demo_class>
make_demo_class(context_intf& ctx, int x, int y)
{
    co_return demo_class(x, y);
}

cppcoro::task<demo_class>
copy_demo_class(context_intf& ctx, demo_class d)
{
    co_return demo_class(d);
}

} // namespace cradle
