#ifndef CRADLE_TESTS_SUPPORT_INNER_SERVICE_H
#define CRADLE_TESTS_SUPPORT_INNER_SERVICE_H

#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

service_config
make_inner_tests_config();

void
init_test_inner_service(inner_resources& resources);

void
inner_reset_disk_cache(inner_resources& resources);

struct uncached_request_resolution_context : public context_intf
{
    bool
    remotely() const override
    {
        return false;
    }
};

struct cached_request_resolution_context : public cached_context_intf
{
    inner_resources resources;

    cached_request_resolution_context();

    inner_resources&
    get_resources() override
    {
        return resources;
    }

    immutable_cache&
    get_cache() override
    {
        return resources.memory_cache();
    }

    bool
    remotely() const override
    {
        return false;
    }

    void
    reset_memory_cache();
};

template<caching_level_type level>
struct request_resolution_context_struct
{
    using type = cached_request_resolution_context;
};

template<>
struct request_resolution_context_struct<caching_level_type::none>
{
    using type = uncached_request_resolution_context;
};

template<caching_level_type level>
using request_resolution_context =
    typename request_resolution_context_struct<level>::type;

// Ensure that the "remote" loopback service is available
void
ensure_loopback_service();

// Ensure that an rpclib client and server are available; returns the client
std::shared_ptr<rpclib_client>
ensure_rpclib_service();

} // namespace cradle

#endif
