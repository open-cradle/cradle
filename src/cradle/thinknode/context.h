#ifndef CRADLE_THINKNODE_CONTEXT_H
#define CRADLE_THINKNODE_CONTEXT_H

#include <cradle/inner/requests/context_base.h>
#include <cradle/inner/service/config.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct thinknode_request_context final : public sync_context_base
{
    service_core& service;
    thinknode_session session;

    // Constructor called from thinknode_domain::make_local_sync_context()
    thinknode_request_context(
        service_core& service, service_config const& config);

    // Other-purposes constructor
    thinknode_request_context(
        service_core& service,
        thinknode_session session,
        tasklet_tracker* tasklet,
        std::string proxy_name);

    ~thinknode_request_context();

    // context_intf
    std::string const&
    domain_name() const override;

    // remote_context_intf
    service_config
    make_config(bool need_record_lock) const override;

    // Other
    std::string const&
    api_url() const
    {
        return session.api_url;
    }
};
static_assert(ValidFinalContext<thinknode_request_context>);

} // namespace cradle

#endif
