#ifndef CRADLE_INNER_REMOTE_LOOPBACK_H
#define CRADLE_INNER_REMOTE_LOOPBACK_H

namespace cradle {

/**
 * Registers the loopback service
 *
 * The loopback service simulates a remote executor, but acts locally.
 * It still resolves serialized requests into serialized responses.
 */
void
register_loopback_service();

} // namespace cradle

#endif
