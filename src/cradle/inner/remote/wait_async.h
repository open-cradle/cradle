#ifndef CRADLE_INNER_REMOTE_WAIT_ASYNC_H
#define CRADLE_INNER_REMOTE_WAIT_ASYNC_H

#include <string>

#include <spdlog/spdlog.h>

#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/generic.h>

// Functionality to block the caller until the context on a remote has reached
// a state corresponding to the passed matcher.

namespace cradle {

class async_status_matcher
{
 public:
    async_status_matcher(std::string name, spdlog::logger& logger);

    virtual ~async_status_matcher() = default;

    virtual bool operator()(async_status) const = 0;

    void
    report_status(async_status status, bool done) const;

 private:
    std::string name_;
    spdlog::logger& logger_;
};

// Polls the status of the remote context for remote_id until it passes the
// matcher's condition. Throws if the remote operation was cancelled or ran
// into an error.
void
wait_until_async_status_matches(
    remote_proxy& proxy,
    async_id remote_id,
    async_status_matcher const& matcher);

} // namespace cradle

#endif
