#ifndef CRADLE_THINKNODE_CONTEXT_H
#define CRADLE_THINKNODE_CONTEXT_H

#include <cradle/inner/requests/context_base.h>
#include <cradle/thinknode/types.hpp>
#include <cradle/typing/service/core.h>

namespace cradle {

struct thinknode_request_context final : public sync_context_base
{
    service_core& service;
    thinknode_session session;

    thinknode_request_context(
        service_core& service,
        thinknode_session session,
        tasklet_tracker* tasklet,
        bool remotely,
        std::string proxy_name);

    ~thinknode_request_context();

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<local_context_intf>
    local_clone() const override;

    std::string const&
    api_url() const
    {
        return session.api_url;
    }

 private:
    std::string domain_name_{"thinknode"};
};
static_assert(ValidContext<thinknode_request_context>);

} // namespace cradle

#endif
