#include <map>
#include <stdexcept>
#include <string>
#include <thread>

#include <cppcoro/schedule_on.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/core/monitoring.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/resources_impl.h>

namespace cradle {

static std::map<std::string, std::unique_ptr<secondary_cache_factory>>
    secondary_cache_factories;

void
register_secondary_cache_factory(
    std::string const& key, std::unique_ptr<secondary_cache_factory> factory)
{
    secondary_cache_factories.emplace(key, std::move(factory));
}

static immutable_cache_config
make_immutable_cache_config(service_config const& config)
{
    return immutable_cache_config{
        .unused_size_limit = config.get_number_or_default(
            inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00)};
}

inner_resources::inner_resources() = default;

inner_resources::~inner_resources() = default;

void
inner_resources::inner_initialize(service_config const& config)
{
    impl_ = std::make_unique<inner_resources_impl>(*this, config);
}

void
inner_resources::reset_memory_cache()
{
    impl_->reset_memory_cache();
}

void
inner_resources::reset_memory_cache(service_config const& config)
{
    impl_->reset_memory_cache(config);
}

void
inner_resources::reset_secondary_cache(service_config const& config)
{
    impl_->reset_secondary_cache(config);
}

cradle::immutable_cache&
inner_resources::memory_cache()
{
    return impl_->memory_cache();
}

secondary_cache_intf&
inner_resources::secondary_cache()
{
    return impl_->secondary_cache();
}

std::shared_ptr<blob_file_writer>
inner_resources::make_blob_file_writer(std::size_t size)
{
    return impl_->make_blob_file_writer(size);
}

http_connection_interface&
http_connection_for_thread(inner_resources& resources)
{
    return resources.impl().http_connection_for_thread();
}

cppcoro::task<http_response>
async_http_request(
    inner_resources& resources, http_request request, tasklet_tracker* client)
{
    return resources.impl().async_http_request(std::move(request), client);
}

mock_http_session&
enable_http_mocking(inner_resources& resources, bool http_is_synchronous)
{
    return resources.impl().enable_http_mocking(http_is_synchronous);
}

inner_resources_impl::inner_resources_impl(
    inner_resources& resources, service_config const& config)
    : blob_dir_{std::make_unique<blob_file_directory>(config)},
      http_pool_{cppcoro::static_thread_pool(
          static_cast<uint32_t>(config.get_number_or_default(
              inner_config_keys::HTTP_CONCURRENCY, 36)))}
{
    create_memory_cache(config);
    create_secondary_cache(resources, config);
}

void
inner_resources_impl::reset_memory_cache()
{
    service_config_map config_map;
    reset_memory_cache(service_config(config_map));
}

void
inner_resources_impl::reset_memory_cache(service_config const& config)
{
    memory_cache_->reset(make_immutable_cache_config(config));
}

void
inner_resources_impl::reset_secondary_cache(service_config const& config)
{
    secondary_cache_->reset(config);
}

std::shared_ptr<blob_file_writer>
inner_resources_impl::make_blob_file_writer(std::size_t size)
{
    auto path{blob_dir_->allocate_file()};
    return std::make_shared<blob_file_writer>(path, size);
}

http_connection_interface&
inner_resources_impl::http_connection_for_thread()
{
    if (mock_http_)
    {
        if (http_is_synchronous_)
        {
            return mock_http_->synchronous_connection();
        }
        else
        {
            thread_local mock_http_connection the_connection(*mock_http_);
            return the_connection;
        }
    }
    else
    {
        static http_request_system the_system;
        thread_local http_connection the_connection(the_system);
        return the_connection;
    }
}

cppcoro::task<http_response>
inner_resources_impl::async_http_request(
    http_request request, tasklet_tracker* client)
{
    std::ostringstream s;
    s << "HTTP: " << request.method << " " << request.url;
    auto tasklet = create_tasklet_tracker("HTTP", s.str(), client);
    if (!http_is_synchronous_)
    {
        co_await http_pool_.schedule();
    }
    tasklet_run tasklet_run(tasklet);
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return http_connection_for_thread().perform_request(
        check_in, reporter, request);
}

mock_http_session&
inner_resources_impl::enable_http_mocking(bool http_is_synchronous)
{
    if (!mock_http_)
    {
        mock_http_ = std::make_unique<mock_http_session>();
        http_is_synchronous_ = http_is_synchronous;
    }
    return *mock_http_;
}

void
inner_resources_impl::create_memory_cache(service_config const& config)
{
    memory_cache_ = std::make_unique<immutable_cache>(
        make_immutable_cache_config(config));
}

void
inner_resources_impl::create_secondary_cache(
    inner_resources& resources, service_config const& config)
{
    auto key = config.get_mandatory_string(
        inner_config_keys::SECONDARY_CACHE_FACTORY);
    auto factory = secondary_cache_factories.find(key);
    if (factory == secondary_cache_factories.end())
    {
        throw config_error{
            fmt::format("No secondary cache factory \"{}\"", key)};
    }
    secondary_cache_ = factory->second->create(resources, config);
}

} // namespace cradle
