#ifndef CRADLE_THINKNODE_IPC_H
#define CRADLE_THINKNODE_IPC_H

#include <boost/function.hpp>
#include <boost/shared_array.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/thinknode/messages.h>
#include <cradle/thinknode/types.hpp>
#include <cradle/typing/core.h>

namespace cradle {

// The following describe the protocol between the calculation supervisor and
// the calculation provider.

enum class calc_message_code : uint8_t
{
    REGISTER = 0,
    FUNCTION,
    PROGRESS,
    RESULT,
    FAILURE,
    PING,
    PONG
};

// The following interface is required of messages that are going to be read
// from the TCP messaging system.

// supervisor messages

void
read_message_body(
    thinknode_supervisor_message* message,
    uint8_t code,
    std::shared_ptr<data_owner> const& body,
    uint8_t const* data,
    size_t length);

// provider messsages

void
read_message_body(
    thinknode_provider_message* message,
    uint8_t code,
    std::shared_ptr<data_owner> const& body,
    uint8_t const* data,
    size_t length);

// The following interface is required of messages that are to be written out
// through the TCP messaging system.

// supervisor messages

calc_message_code
get_message_code(thinknode_supervisor_message const& message);

size_t
get_message_body_size(thinknode_supervisor_message const& message);

void
write_message_body(
    tcp::socket& socket, thinknode_supervisor_message const& message);

// provider messages

calc_message_code
get_message_code(thinknode_provider_message const& message);

size_t
get_message_body_size(thinknode_provider_message const& message);

void
write_message_body(
    tcp::socket& socket, thinknode_provider_message const& message);

} // namespace cradle

#endif
