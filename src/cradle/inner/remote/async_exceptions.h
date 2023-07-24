#ifndef CRADLE_INNER_REMOTE_ASYNC_EXCEPTIONS_H
#define CRADLE_INNER_REMOTE_ASYNC_EXCEPTIONS_H

#include <stdexcept>

namespace cradle {

// Thrown (by an RPC server) if an invalid async_id was specified
// (by the client)
class bad_async_id_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

} // namespace cradle

#endif
