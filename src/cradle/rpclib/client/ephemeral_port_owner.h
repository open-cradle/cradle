#ifndef CRADLE_RPCLIB_CLIENT_EPHEMERAL_PORT_OWNER_H
#define CRADLE_RPCLIB_CLIENT_EPHEMERAL_PORT_OWNER_H

#include <bitset>
#include <memory>
#include <mutex>

#include <cradle/rpclib/common/port.h>

namespace cradle {

// Owns a range of ephemeral ports, and allocates ports from that range.
// An ephemeral port is one that a contained process (an rpclib server running
// in contained mode) listens on. The port is allocated on behalf of that
// process, and freed when the process is killed.
class ephemeral_port_owner
{
 public:
    rpclib_port_t
    alloc_port();

    void
    free_port(rpclib_port_t port_number);

 private:
    static constexpr std::size_t first_number_{49152};
    static constexpr std::size_t range_size_{256};
    std::mutex mutex_;
    std::size_t next_number_{first_number_};
    std::bitset<first_number_ + range_size_> in_use_;

    std::size_t
    find_unused_number() const;
    std::size_t
    increase(std::size_t n) const;
};

} // namespace cradle

#endif
