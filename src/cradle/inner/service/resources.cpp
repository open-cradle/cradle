#include <map>
#include <stdexcept>
#include <string>
#include <thread>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/core/monitoring.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/introspection/config.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/resources_impl.h>
#include <cradle/inner/service/secondary_storage_intf.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

static immutable_cache_config
make_immutable_cache_config(service_config const& config)
{
    return immutable_cache_config{
        .unused_size_limit = config.get_number_or_default(
            inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00)};
}

static std::unique_ptr<cradle::immutable_cache>
create_memory_cache(service_config const& config)
{
    return std::make_unique<immutable_cache>(
        make_immutable_cache_config(config));
}

inner_resources::inner_resources(service_config const& config)
    : impl_{std::make_unique<inner_resources_impl>(*this, config)}
{
}

// The destructor needs the inner_resources_impl definition, which is not
// available in resources.h.
inner_resources::~inner_resources() = default;

service_config const&
inner_resources::config() const
{
    return impl_->config_;
}

cradle::immutable_cache&
inner_resources::memory_cache()
{
    return *impl_->memory_cache_;
}

void
inner_resources::reset_memory_cache()
{
    auto& impl{*impl_};
    impl.logger_->info("reset memory cache");
    impl.memory_cache_->reset(make_immutable_cache_config(impl.config_));
}

void
inner_resources::set_secondary_cache(
    std::unique_ptr<secondary_storage_intf> secondary_cache)
{
    auto& impl{*impl_};
    if (impl.secondary_cache_)
    {
        throw std::logic_error(
            "attempt to change secondary cache in inner_resources");
    }
    impl.secondary_cache_ = std::move(secondary_cache);
}

secondary_storage_intf&
inner_resources::secondary_cache()
{
    auto& impl{*impl_};
    if (!impl.secondary_cache_)
    {
        throw std::logic_error("inner_resources has no secondary cache");
    }
    return *impl.secondary_cache_;
}

void
inner_resources::clear_secondary_cache()
{
    secondary_cache().clear();
}

void
inner_resources::set_requests_storage(
    std::unique_ptr<secondary_storage_intf> requests_storage)
{
    auto& impl{*impl_};
    if (impl.requests_storage_)
    {
        throw std::logic_error(
            "attempt to change requests storage in inner_resources");
    }
    impl.requests_storage_ = std::move(requests_storage);
}

secondary_storage_intf&
inner_resources::requests_storage()
{
    auto& impl{*impl_};
    if (!impl.requests_storage_)
    {
        throw std::logic_error("inner_resources has no requests storage");
    }
    return *impl.requests_storage_;
}

http_connection_interface&
inner_resources::http_connection_for_thread()
{
    return impl_->http_connection_for_thread(nullptr);
}

cppcoro::task<http_response>
inner_resources::async_http_request(
    http_request request, tasklet_tracker* client)
{
    auto& impl{*impl_};
    std::ostringstream s;
    s << "HTTP: " << request.method << " " << request.url;
    auto tasklet
        = create_tasklet_tracker(the_tasklet_admin(), "HTTP", s.str(), client);
    if (!impl.http_is_synchronous_)
    {
        co_await impl.http_pool_.schedule();
    }
    tasklet_run tasklet_run(tasklet);
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return impl.http_connection_for_thread(&request).perform_request(
        check_in, reporter, request);
}

mock_http_session&
inner_resources::enable_http_mocking(bool http_is_synchronous)
{
    auto& impl{*impl_};
    if (!impl.mock_http_)
    {
        impl.mock_http_ = std::make_unique<mock_http_session>();
        impl.http_is_synchronous_ = http_is_synchronous;
    }
    return *impl.mock_http_;
}

std::shared_ptr<blob_file_writer>
inner_resources::make_blob_file_writer(std::size_t size)
{
    auto path{impl_->blob_dir_->allocate_file()};
    return std::make_shared<blob_file_writer>(path, size);
}

void
inner_resources::ensure_async_db()
{
    auto& impl{*impl_};
    std::scoped_lock lock{impl.mutex_};
    if (!impl.the_async_db_)
    {
        impl.the_async_db_ = std::make_unique<async_db>();
    }
}

async_db*
inner_resources::get_async_db()
{
    return impl_->the_async_db_.get();
}

cppcoro::static_thread_pool&
inner_resources::get_async_thread_pool()
{
    return impl_->async_pool_;
}

void
inner_resources::register_domain(std::unique_ptr<domain> dom)
{
    std::string dom_name{dom->name()};
    impl_->domains_.insert(std::make_pair(dom_name, std::move(dom)));
}

domain&
inner_resources::find_domain(std::string const& name)
{
    auto& impl{*impl_};
    auto it = impl.domains_.find(name);
    if (it == impl.domains_.end())
    {
        throw std::domain_error(fmt::format("unknown domain {}", name));
    }
    return *it->second;
}

void
inner_resources::register_proxy(std::unique_ptr<remote_proxy> proxy)
{
    auto& impl{*impl_};
    std::string const& name{proxy->name()};
    if (impl.proxies_.contains(name))
    {
        throw std::logic_error{
            fmt::format("Proxy {} already registered", name)};
    }
    impl.proxies_[name] = std::move(proxy);
}

remote_proxy&
inner_resources::get_proxy(std::string const& name)
{
    auto& impl{*impl_};
    auto it = impl.proxies_.find(name);
    if (it == impl.proxies_.end())
    {
        throw std::logic_error{fmt::format("Proxy {} not registered", name)};
    }
    return *it->second;
}

dll_collection&
inner_resources::the_dlls()
{
    return impl_->the_dlls_;
}

std::shared_ptr<seri_registry>
inner_resources::get_seri_registry()
{
    return impl_->the_seri_registry_;
}

tasklet_admin&
inner_resources::the_tasklet_admin()
{
    return impl_->the_tasklet_admin_;
}

inner_resources_impl::inner_resources_impl(
    inner_resources& wrapper, service_config const& config)
    : config_{config},
      logger_{ensure_logger("svc")},
      memory_cache_{create_memory_cache(config)},
      blob_dir_{std::make_unique<blob_file_directory>(config)},
      the_seri_registry_{std::make_unique<seri_registry>()},
      the_dlls_{wrapper},
      the_tasklet_admin_{config.get_bool_or_default(
          introspection_config_keys::FORCE_FINISH, false)},
      http_pool_{cppcoro::static_thread_pool(
          static_cast<uint32_t>(config.get_number_or_default(
              inner_config_keys::HTTP_CONCURRENCY, 36)))},
      async_pool_{cppcoro::static_thread_pool(
          static_cast<uint32_t>(config.get_number_or_default(
              inner_config_keys::ASYNC_CONCURRENCY, 20)))}
{
}

http_connection_interface&
inner_resources_impl::http_connection_for_thread(http_request const* request)
{
    if (mock_http_
        && (request == nullptr || mock_http_->enabled_for(*request)))
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

} // namespace cradle
