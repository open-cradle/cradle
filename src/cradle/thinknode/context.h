#ifndef CRADLE_THINKNODE_CONTEXT_H
#define CRADLE_THINKNODE_CONTEXT_H

#include <vector>

#include <cradle/inner/requests/generic.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct immutable_cache;
class service_core;
class tasklet_tracker;

struct thinknode_request_context final : public remote_context_intf,
                                         public local_context_intf,
                                         public sync_context_intf,
                                         public caching_context_intf,
                                         public introspective_context_intf
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

    inner_resources&
    get_resources() override;

    tasklet_tracker*
    get_tasklet() override;

    void
    push_tasklet(tasklet_tracker* tasklet) override;

    void
    pop_tasklet() override;

    bool
    remotely() const override
    {
        return remotely_;
    }

    bool
    is_async() const override
    {
        return false;
    }

    std::string const&
    proxy_name() const override
    {
        return proxy_name_;
    }

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
    std::vector<tasklet_tracker*> tasklets_;
    bool remotely_;
    std::string proxy_name_;
    std::string domain_name_{"thinknode"};
};

static_assert(ValidContext<thinknode_request_context>);

} // namespace cradle

#endif
