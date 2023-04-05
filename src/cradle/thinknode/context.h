#ifndef CRADLE_THINKNODE_CONTEXT_H
#define CRADLE_THINKNODE_CONTEXT_H

#include <vector>

#include <cradle/inner/requests/generic.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct immutable_cache;
class service_core;
class tasklet_tracker;

struct thinknode_request_context : public cached_introspective_context_intf,
                                   public remote_context_intf
{
    service_core& service;
    thinknode_session session;

    thinknode_request_context(
        service_core& service,
        thinknode_session session,
        tasklet_tracker* tasklet,
        bool remotely = false);

    ~thinknode_request_context();

    inner_resources&
    get_resources() override;

    immutable_cache&
    get_cache() override;

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

    void
    proxy_name(std::string const& name);

    std::string const&
    proxy_name() const override;

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::shared_ptr<remote_context_intf>
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

} // namespace cradle

#endif
