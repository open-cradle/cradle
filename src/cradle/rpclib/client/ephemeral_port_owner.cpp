#include <stdexcept>

#include <fmt/format.h>

#include <cradle/rpclib/client/ephemeral_port_owner.h>

namespace cradle {

rpclib_port_t
ephemeral_port_owner::alloc_port()
{
    std::scoped_lock lock{mutex_};
    auto n = find_unused_number();
    in_use_[n - first_number_] = true;
    next_number_ = increase(n);
    return static_cast<rpclib_port_t>(n);
}

void
ephemeral_port_owner::free_port(rpclib_port_t port_number)
{
    if (port_number < first_number_
        || port_number >= first_number_ + range_size_)
    {
        throw std::out_of_range{fmt::format("bad port #{}", port_number)};
    }
    auto pos = port_number - first_number_;
    std::scoped_lock lock{mutex_};
    if (!in_use_[pos])
    {
        throw std::logic_error{
            fmt::format("port #{} not in use", port_number)};
    }
    in_use_[pos] = false;
}

std::size_t
ephemeral_port_owner::find_unused_number() const
{
    auto n = next_number_;
    for (;;)
    {
        if (!in_use_[n - first_number_])
        {
            break;
        }
        n = increase(n);
        if (n == next_number_)
        {
            throw std::length_error("no unused emphemeral port");
        }
    }
    return n;
}

std::size_t
ephemeral_port_owner::increase(std::size_t n) const
{
    auto n1 = n + 1;
    if (n1 == first_number_ + range_size_)
    {
        n1 = first_number_;
    }
    return n1;
}

} // namespace cradle
