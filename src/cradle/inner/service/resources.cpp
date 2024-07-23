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
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/common/config.h>

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
    if (config.get_bool_or_default(rpclib_config_keys::CONTAINED, false))
    {
        // No memory cache in contained mode
        return {};
    }
    return std::make_unique<immutable_cache>(
        make_immutable_cache_config(config));
}

static inner_resources* current_inner_resources{nullptr};

inner_resources&
get_current_inner_resources()
{
    if (!current_inner_resources)
    {
        throw std::logic_error{"no current_inner_resources"};
    }
    return *current_inner_resources;
}

inner_resources::inner_resources(service_config const& config)
    : impl_{std::make_unique<inner_resources_impl>(*this, config)}
{
    // The critical situation is with a loopback, when we legitimately have two
    // sets of resources in the same process, and the function_request
    // reconstruction code cannot know which one it should use.
    // Decision: use the first one, which is not the one for the loopback.
    // This will work unless the loopback resolves via a contained process.
    if (current_inner_resources)
    {
        auto& logger{impl_->the_logger()};
        logger.info("inner_resources ctor found existing instance");
    }
    else
    {
        current_inner_resources = this;
    }
}

inner_resources::~inner_resources()
{
    current_inner_resources = nullptr;
}

service_config const&
inner_resources::config() const
{
    return impl_->config_;
}

bool
inner_resources::support_caching() const
{
    auto& impl{*impl_};
    return impl.memory_cache_.operator bool();
}

cradle::immutable_cache&
inner_resources::memory_cache()
{
    auto& impl{*impl_};
    impl.check_support_caching();
    return *impl.memory_cache_;
}

void
inner_resources::reset_memory_cache()
{
    auto& impl{*impl_};
    impl.check_support_caching();
    impl.logger_->info("reset memory cache");
    impl.memory_cache_->reset(make_immutable_cache_config(impl.config_));
}

void
inner_resources::set_secondary_cache(
    std::unique_ptr<secondary_storage_intf> secondary_cache)
{
    auto& impl{*impl_};
    impl.check_support_caching();
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

seri_cache_record_lock_t
inner_resources::alloc_cache_record_lock()
{
    auto& impl{*impl_};
    std::scoped_lock lock{impl.mutex_};
    auto record_id = impl_->next_remote_record_id_++;
    auto record_lock = std::make_unique<cache_record_lock>();
    auto lock_ptr = &*record_lock;
    impl_->cache_record_locks_.insert(
        std::make_pair(record_id.value(), std::move(record_lock)));
    return seri_cache_record_lock_t{lock_ptr, record_id};
}

void
inner_resources::release_cache_record_lock(remote_cache_record_id record_id)
{
    auto key = record_id.value();
    auto num_removed = impl_->cache_record_locks_.erase(key);
    if (num_removed == 0)
    {
        impl_->logger_->error(
            "release_cache_record_lock(): invalid record_id {}", key);
    }
}

tasklet_admin&
inner_resources::the_tasklet_admin()
{
    return impl_->the_tasklet_admin_;
}

cppcoro::io_service&
inner_resources::the_io_service()
{
    return impl_->io_svc_;
}

std::unique_ptr<rpclib_client>
inner_resources::alloc_contained_proxy(std::shared_ptr<spdlog::logger> logger)
{
    return impl_->contained_proxy_pool_.alloc_proxy(impl_->config_, logger);
}

void
inner_resources::free_contained_proxy(
    std::unique_ptr<rpclib_client> proxy, bool succeeded)
{
    impl_->contained_proxy_pool_.free_proxy(std::move(proxy), succeeded);
}

int
inner_resources::get_num_contained_calls() const
{
    return impl_->num_contained_calls_;
}

void
inner_resources::increase_num_contained_calls()
{
    impl_->num_contained_calls_ += 1;
}

static void
io_svc_func(inner_resources_impl& impl)
{
    auto& io_svc{impl.the_io_service()};
    auto& logger{impl.the_logger()};
    logger.info("io_svc_func start");
    auto num_events = io_svc.process_events();
    logger.info("io_svc_func stop; got {} events", num_events);
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
      io_svc_thread_{io_svc_func, std::ref(*this)},
      http_pool_{cppcoro::static_thread_pool(
          static_cast<uint32_t>(config.get_number_or_default(
              inner_config_keys::HTTP_CONCURRENCY, 36)))},
      async_pool_{cppcoro::static_thread_pool(
          static_cast<uint32_t>(config.get_number_or_default(
              inner_config_keys::ASYNC_CONCURRENCY, 20)))}
{
}

inner_resources_impl::~inner_resources_impl()
{
    auto& logger{*logger_};
    logger.info("stopping io_svc");
    io_svc_.stop(); // noexcept
    logger.info("stopped io_svc");
    try
    {
        io_svc_thread_.join();
    }
    catch (std::exception const& e)
    {
        logger.info("join threw {}", e.what());
    }
    logger.info("joined io_svc_thread_");
}

void
inner_resources_impl::check_support_caching()
{
    if (!memory_cache_)
    {
        throw std::logic_error("caching not supported in contained mode");
    }
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
