#ifndef CRADLE_TESTS_SUPPORT_INNER_SERVICE_H
#define CRADLE_TESTS_SUPPORT_INNER_SERVICE_H

#include "common.h"
#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

service_config
make_inner_tests_config();

inner_resources
make_inner_test_resources(
    std::string const& proxy_name = {},
    domain_option const& domain = no_domain_option());

void
clear_disk_cache(inner_resources& resources);

struct non_caching_request_resolution_context final
    : public local_context_intf,
      public sync_context_intf
{
    // context_intf
    bool
    remotely() const override
    {
        return false;
    }

    bool
    is_async() const override
    {
        return false;
    }

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override
    {
        throw not_implemented_error();
    }

    void
    on_value_complete() override
    {
        throw not_implemented_error();
    }
};

static_assert(ValidContext<non_caching_request_resolution_context>);

struct caching_request_resolution_context final : public local_context_intf,
                                                  public sync_context_intf,
                                                  public caching_context_intf
{
    inner_resources resources;

    caching_request_resolution_context();

    // context_intf
    bool
    remotely() const override
    {
        return false;
    }

    bool
    is_async() const override
    {
        return false;
    }

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override
    {
        throw not_implemented_error();
    }

    void
    on_value_complete() override
    {
        throw not_implemented_error();
    }

    // caching_context_intf
    inner_resources&
    get_resources() override
    {
        return resources;
    }

    // other
    void
    reset_memory_cache();
};

static_assert(ValidContext<caching_request_resolution_context>);

template<caching_level_type level>
struct request_resolution_context_struct
{
    using type = caching_request_resolution_context;
};

template<>
struct request_resolution_context_struct<caching_level_type::none>
{
    using type = non_caching_request_resolution_context;
};

template<caching_level_type level>
using request_resolution_context =
    typename request_resolution_context_struct<level>::type;

} // namespace cradle

#endif
