#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LOCAL_DURABLE_STORE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LOCAL_DURABLE_STORE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <BS_thread_pool.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/service/config.h>
#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

namespace cradle {

// Config values naming the durable (on-disk) backends, following the
// *_config_values::PLUGIN_NAME convention and mirroring the M1
// storage_config_values::MEMORY_* value names.
struct durable_storage_config_values
{
    inline static std::string const DISK_CAS{"disk_cas"};
    inline static std::string const DISK_AC{"disk_ac"};
    inline static std::string const DISK_MUTABLE_STORE{"disk_mutable_store"};
};

// Optional per-capability location/capacity/integrity overrides layered on top
// of the shared local_disk_cache_config_keys. When an override is absent the
// shared default applies (disk_cache/directory, disk_cache/size_limit,
// disk_cache/check_file_data). start_empty has no per-capability form and is
// always taken from the shared disk_cache/start_empty default.
struct durable_storage_config_keys
{
    // Per-capability directory overrides: disk_cache/<capability>/directory.
    inline static std::string const CAS_DIRECTORY{"disk_cache/cas/directory"};
    inline static std::string const AC_DIRECTORY{"disk_cache/ac/directory"};
    inline static std::string const MUTABLE_STORE_DIRECTORY{
        "disk_cache/mutable_store/directory"};

    // Per-capability size-limit overrides: disk_cache/<capability>/size_limit.
    inline static std::string const CAS_SIZE_LIMIT{
        "disk_cache/cas/size_limit"};
    inline static std::string const AC_SIZE_LIMIT{"disk_cache/ac/size_limit"};
    inline static std::string const MUTABLE_STORE_SIZE_LIMIT{
        "disk_cache/mutable_store/size_limit"};

    // Per-capability on-read integrity overrides:
    // disk_cache/<capability>/check_file_data.
    inline static std::string const CAS_CHECK_FILE_DATA{
        "disk_cache/cas/check_file_data"};
    inline static std::string const AC_CHECK_FILE_DATA{
        "disk_cache/ac/check_file_data"};
    inline static std::string const MUTABLE_STORE_CHECK_FILE_DATA{
        "disk_cache/mutable_store/check_file_data"};
};

// A single location's fully-resolved durable-store settings, produced by
// resolving the shared disk_cache/* defaults against any per-capability
// overrides. Two capabilities that resolve to the same directory yield
// settings with the same directory and therefore share one store.
struct durable_store_settings
{
    std::string directory;
    std::optional<size_t> size_limit;
    bool check_file_data;
    bool start_empty;
};

// Resolve the effective settings for one capability ("cas", "ac", or
// "mutable_store"): directory, size_limit, and check_file_data come from the
// per-capability override when present, otherwise from the shared disk_cache/*
// default; start_empty always comes from the shared default.
durable_store_settings
resolve_durable_store_settings(
    service_config const& config, std::string const& capability);

// Owns one ll_disk_cache at a configured location plus the I/O thread pools
// shared by the durable adapters that reference it. Constructed either from a
// service_config using the shared local_disk_cache_config_keys defaults, or
// from fully-resolved, capability-scoped settings so a single capability can
// be assigned its own on-disk store. Internally thread-safe via
// ll_disk_cache's own synchronization.
class local_durable_store
{
 public:
    explicit local_durable_store(service_config const& config);

    explicit local_durable_store(durable_store_settings const& settings);

    // The shared low-level on-disk store.
    ll_disk_cache&
    cache();

    // Pool for offloading blocking reads.
    cppcoro::static_thread_pool&
    read_pool();

    // Pool for offloading blocking writes.
    BS::thread_pool&
    write_pool();

    // Whether on-read integrity verification is enabled for this store.
    bool
    check_file_data() const;

 private:
    bool check_file_data_;
    ll_disk_cache ll_cache_;
    cppcoro::static_thread_pool read_pool_;
    BS::thread_pool write_pool_;
    std::shared_ptr<spdlog::logger> logger_;
};

// Factory: builds a shared durable store from configuration (shared defaults).
std::shared_ptr<local_durable_store>
make_local_durable_store(service_config const& config);

// Factory: builds a durable store from fully-resolved, capability-scoped
// settings, used when a capability resolves to its own distinct directory.
std::shared_ptr<local_durable_store>
make_local_durable_store(durable_store_settings const& settings);

} // namespace cradle

#endif
