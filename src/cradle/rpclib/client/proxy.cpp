#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <fmt/format.h>
#include <rpc/client.h>
#include <rpc/rpc_error.h>

#include <cradle/deploy_dir.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_adaptors_rpclib.h>
#include <cradle/inner/service/config_map_to_json.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/rpclib/client/ephemeral_port_owner.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/rpclib/common/config.h>

namespace cradle {

namespace {

// Retrieves the message associated with the RPC error: whatever the server
// passed to respond_error(), converted to a string
std::string
get_message(rpc::rpc_error& exc)
{
    std::string msg;
    try
    {
        // Don't rely on operator<< to convert msgpack strings
        // (it will embed a string in quotes).
        msg = exc.get_error().as<std::string>();
    }
    catch (std::bad_cast&)
    {
        auto obj = exc.get_error().get();
        std::ostringstream os;
        os << obj;
        msg = os.str();
    }
    return msg;
}

// Returns an indication whether it would make sense to retry the request that
// caused the given error.
//
// Returns true if the error was raised by the rpclib library, or
// (intentionally) looks like it was.
// An error raised by the rpclib library is sent as a string starting with
// "rpclib: ".
bool
is_retryable(rpc::rpc_error& exc)
{
    bool retryable{false};
    try
    {
        auto msg = exc.get_error().as<std::string>();
        retryable = msg.starts_with("rpclib: ");
    }
    catch (std::bad_cast&)
    {
        // Unrecognized error (not a string): don't retry
    }
    return retryable;
}

bool
get_testing(service_config const& config)
{
    return config.get_bool_or_default(generic_config_keys::TESTING, false);
}

rpclib_port_t
alloc_port(ephemeral_port_owner* port_owner, service_config const& config)
{
    if (port_owner)
    {
        return port_owner->alloc_port();
    }
    auto opt_port
        = config.get_optional_number(rpclib_config_keys::PORT_NUMBER);
    if (opt_port)
    {
        return static_cast<rpclib_port_t>(*opt_port);
    }
    return get_testing(config) ? RPCLIB_PORT_TESTING : RPCLIB_PORT_PRODUCTION;
}

} // namespace

rpclib_client::rpclib_client(
    service_config const& config,
    ephemeral_port_owner* port_owner,
    std::shared_ptr<spdlog::logger> logger)
    : pimpl_{std::make_unique<rpclib_client_impl>(
        config, port_owner, std::move(logger))}
{
}

rpclib_client::~rpclib_client() = default;

rpclib_client_impl::rpclib_client_impl(
    service_config const& config,
    ephemeral_port_owner* port_owner,
    std::shared_ptr<spdlog::logger> logger)
    : port_owner_{port_owner},
      logger_{logger ? std::move(logger) : ensure_logger("rpclib_client")},
      testing_{get_testing(config)},
      contained_{port_owner != nullptr},
      deploy_dir_{config.get_optional_string(generic_config_keys::DEPLOY_DIR)},
      port_{alloc_port(port_owner, config)},
      secondary_cache_factory_{config.get_optional_string(
          inner_config_keys::SECONDARY_CACHE_FACTORY)}
{
    start_server();
}

rpclib_client_impl::~rpclib_client_impl()
{
    try
    {
        stop_server();
        if (port_owner_)
        {
            port_owner_->free_port(port_);
        }
    }
    catch (std::exception const& e)
    {
        logger_->error("caught {}", e.what());
    }
}

rpclib_port_t
rpclib_client::get_port() const
{
    return pimpl_->get_port();
}

std::string
rpclib_client::name() const
{
    return "rpclib";
}

spdlog::logger&
rpclib_client::get_logger()
{
    return *pimpl_->logger_;
}

serialized_result
rpclib_client::resolve_sync(service_config config, std::string seri_req)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("resolve_sync");
    auto response = pimpl_
                        ->do_rpc_call(
                            "resolve_sync",
                            -1,
                            write_config_map_to_json(config.get_config_map()),
                            seri_req)
                        .as<rpclib_response>();
    return pimpl_->make_serialized_result(response);
}

async_id
rpclib_client::submit_async(service_config config, std::string seri_req)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("submit_async");
    auto aid = pimpl_
                   ->do_rpc_call(
                       "submit_async",
                       pimpl_->default_timeout,
                       write_config_map_to_json(config.get_config_map()),
                       seri_req)
                   .as<async_id>();
    logger.debug("submit_async -> {}", aid);
    return aid;
}

remote_context_spec_list
rpclib_client::get_sub_contexts(async_id aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_sub_contexts {}", aid);
    auto result
        = pimpl_->do_rpc_call("get_sub_contexts", pimpl_->default_timeout, aid)
              .as<remote_context_spec_list>();
    logger.debug("get_sub_contexts {} -> {} sub(s)", aid, result.size());
    return result;
}

async_status
rpclib_client::get_async_status(async_id aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_async_status {}", aid);
    auto status_value
        = pimpl_->do_rpc_call("get_async_status", pimpl_->default_timeout, aid)
              .as<int>();
    async_status status{status_value};
    logger.debug("async_status for {}: {}", aid, status);
    return status;
}

std::string
rpclib_client::get_async_error_message(async_id aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_async_error_message {}", aid);
    auto errmsg
        = pimpl_
              ->do_rpc_call(
                  "get_async_error_message", pimpl_->default_timeout, aid)
              .as<std::string>();
    logger.debug("async_error_message for {}: {}", aid, errmsg);
    return errmsg;
}

serialized_result
rpclib_client::get_async_response(async_id root_aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_async_response {}", root_aid);
    auto response = pimpl_
                        ->do_rpc_call(
                            "get_async_response",
                            pimpl_->get_async_response_timeout,
                            root_aid)
                        .as<rpclib_response>();
    return pimpl_->make_serialized_result(response);
}

request_essentials
rpclib_client::get_essentials(async_id aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_essentials {}", aid);
    auto essentials_tuple
        = pimpl_->do_rpc_call("get_essentials", pimpl_->default_timeout, aid)
              .as<rpclib_essentials>();
    auto uuid_str = std::get<0>(essentials_tuple);
    auto opt_title = std::get<1>(essentials_tuple);
    logger.debug(
        "essentials for {}: uuid {}, title {}", aid, uuid_str, opt_title);
    if (opt_title.empty())
    {
        return request_essentials{std::move(uuid_str)};
    }
    else
    {
        return request_essentials{std::move(uuid_str), std::move(opt_title)};
    }
}

void
rpclib_client::request_cancellation(async_id aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("request_cancellation {}", aid);
    pimpl_->do_rpc_call("request_cancellation", pimpl_->default_timeout, aid);
    logger.debug("request_cancellation done");
}

void
rpclib_client::finish_async(async_id root_aid)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("finish_async {}", root_aid);
    pimpl_->do_rpc_call("finish_async", pimpl_->default_timeout, root_aid);
    logger.debug("finish_async done");
}

tasklet_info_list
rpclib_client::get_tasklet_infos(bool include_finished)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_tasklet_infos {}", include_finished);
    auto tuples = pimpl_
                      ->do_rpc_call(
                          "get_tasklet_infos",
                          pimpl_->default_timeout,
                          include_finished)
                      .as<tasklet_info_tuple_list>();
    logger.debug("get_tasklet_infos done");
    return make_tasklet_infos(tuples);
}

void
rpclib_client::load_shared_library(std::string dir_path, std::string dll_name)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("load_shared_library {} {}", dir_path, dll_name);
    std::scoped_lock lock{pimpl_->loaded_dlls_mutex_};
    if (pimpl_->loaded_dlls_.contains(dll_name))
    {
        logger.debug("skip loading DLL {} as it's already there", dll_name);
        return;
    }
    pimpl_->do_rpc_call(
        "load_shared_library", pimpl_->load_dll_timeout, dir_path, dll_name);
    pimpl_->loaded_dlls_.insert(dll_name);
    logger.debug("load_shared_library done");
}

void
rpclib_client::unload_shared_library(std::string dll_name)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("unload_shared_library {}", dll_name);
    std::scoped_lock lock{pimpl_->loaded_dlls_mutex_};
    pimpl_->loaded_dlls_.erase(dll_name);
    pimpl_->do_rpc_call(
        "unload_shared_library", pimpl_->default_timeout, dll_name);
    logger.debug("unload_shared_library done");
}

void
rpclib_client::mock_http(std::string const& response_body)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("mock_http start");
    pimpl_->do_rpc_call("mock_http", pimpl_->default_timeout, response_body);
    logger.debug("mock_http finished");
}

void
rpclib_client::clear_unused_mem_cache_entries()
{
    auto& logger{*pimpl_->logger_};
    logger.debug("clear_unused_mem_cache_entries start");
    pimpl_->do_rpc_call(
        "clear_unused_mem_cache_entries", pimpl_->default_timeout);
    logger.debug("clear_unused_mem_cache_entries finished");
}

void
rpclib_client::release_cache_record_lock(remote_cache_record_id record_id)
{
    auto& logger{*pimpl_->logger_};
    logger.debug("release_cache_record_lock start");
    pimpl_->do_rpc_call(
        "release_cache_record_lock",
        pimpl_->default_timeout,
        record_id.value());
    logger.debug("release_cache_record_lock finished");
}

int
rpclib_client::get_num_contained_calls() const
{
    auto& logger{*pimpl_->logger_};
    logger.debug("get_num_contained_calls start");
    auto num
        = pimpl_
              ->do_rpc_call("get_num_contained_calls", pimpl_->default_timeout)
              .as<int>();
    logger.debug("get_num_contained_calls -> {}", num);
    return num;
}

// Note is blocking
std::string
rpclib_client::ping()
{
    return pimpl_->ping(pimpl_->default_timeout);
}

std::string
rpclib_client_impl::ping(int timeout)
{
    logger_->debug("ping");
    std::string result = do_rpc_call("ping", timeout).as<std::string>();
    logger_->debug("pong {}", result);
    return result;
}

void
rpclib_client::verify_rpclib_protocol(
    std::string const& server_rpclib_protocol)
{
    pimpl_->verify_rpclib_protocol(server_rpclib_protocol);
}

void
rpclib_client_impl::verify_rpclib_protocol(
    std::string const& server_rpclib_protocol)
{
    if (server_rpclib_protocol != RPCLIB_PROTOCOL)
    {
        auto msg{fmt::format(
            "rpclib server has {}, client has {}",
            server_rpclib_protocol,
            RPCLIB_PROTOCOL)};
        logger_->error(msg);
        throw remote_error("rpclib protocol mismatch", msg);
    }
}

// Note is asynchronous
void
rpclib_client_impl::ack_response(uint32_t pool_id)
{
    logger_->debug("ack_response {}", pool_id);
    // It looks more efficient to dispatch the call to another thread, but
    // attempts to do so resulted in resolve_sync hangups of typically 48ms,
    // about every 10 requests, making everything much slower.
    do_rpc_async_call("ack_response", pool_id);
}

bool
rpclib_client_impl::server_is_running()
{
    logger_->info("test whether rpclib server {} is running", port_);
    std::string server_rpclib_protocol;
    try
    {
        rpc_client_ = std::make_unique<rpc::client>(localhost_, port_);
        // Set a timeout that rpc_client_ applies to:
        // - Establishing a connection
        // - Doing a sync RPC call
        // As we do async RPC calls only (see do_rpc_call()), this timeout is
        // just used for establishing a connection.
        rpc_client_->set_timeout(connection_timeout);
        server_rpclib_protocol = ping(default_timeout);
    }
    catch (rpc::system_error const& e)
    {
        // Linux: error code 111, immediately
        // Windows: error code 10061, but only after 2 or more seconds
        // (as per design; cf. TcpMaxConnectRetransmissions)
        // rpclib sets the error code, not what
        logger_->info(
            "rpclib server is not running (code {})", e.code().value());
        return false;
    }
    catch (rpc::rpc_error const&)
    {
        // rpc_error doesn't contain anything useful in this case
        logger_->info("rpclib server is not running (rpc_error)");
        return false;
    }
    catch (remote_error const&)
    {
        // Already reported
        return false;
    }
    logger_->info(
        "received pong {}: rpclib server is running", server_rpclib_protocol);
    // Detect an incompatible rpclib server instance.
    verify_rpclib_protocol(server_rpclib_protocol);
    return true;
}

void
rpclib_client_impl::wait_until_server_running()
{
    int attempt = 0;
    auto t0 = std::chrono::steady_clock::now();
    while (!server_is_running())
    {
        auto t1 = std::chrono::steady_clock::now();
        auto elapsed
            = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                  .count();
        if (elapsed >= detect_server_timeout)
        {
            throw remote_error(
                "could not start rpclib_server", "timeout", true);
        }
        int delay = attempt < 7 ? (1 << attempt) : 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        attempt += 1;
    }
}

void
rpclib_client_impl::start_server()
{
    namespace bf = boost::filesystem;
    namespace bp = boost::process;
    if (server_is_running())
    {
        return;
    }
    std::string server_name{"rpclib_server"};
    std::vector<std::string> child_args{"--log-level", "warn"};
    if (testing_)
    {
        child_args.push_back("--testing");
    }
    if (contained_)
    {
        child_args.push_back("--contained");
    }
    child_args.push_back("--port");
    child_args.push_back(std::to_string(port_));
    if (secondary_cache_factory_)
    {
        child_args.push_back("--secondary-cache");
        child_args.push_back(*secondary_cache_factory_);
    }
    std::string cmd;
    bf::path path;
    if (deploy_dir_)
    {
        cmd = fmt::format("{}/{}{}", *deploy_dir_, server_name, get_exe_ext());
        path = bf::path{cmd};
    }
    else
    {
        cmd = server_name;
        path = bp::search_path(server_name);
    }
    for (auto const& arg : child_args)
    {
        cmd += fmt::format(" {}", arg);
    }
    logger_->info("starting {}", cmd);
    boost::process::child child;
    if (contained_)
    {
        child = bp::child(bp::exe = path, bp::args = child_args);
    }
    else
    {
        child = bp::child(bp::exe = path, bp::args = child_args, group_);
    }
    logger_->info("started child process");
    wait_until_server_running();
    child_ = std::move(child);
}

void
rpclib_client_impl::stop_server()
{
    if (!child_.valid())
    {
        return;
    }
    // In testing mode, a new rpclib server instance is used for each unit
    // test, to have good test isolation.
    // In contained mode, the lifetime of the rpclib server instance is
    // controlled by the lifetime of the corresponding rpclib_client object.
    if (!(testing_ || contained_))
    {
        logger_->info("keep rpclib process running");
        // ~child() will call terminate (without warning) when the child
        // was neither joined nor detached.
        child_.detach();
        // If ~group() is called without a previous detach or wait, the
        // group will be terminated.
        group_.detach();
        return;
    }
    logger_->info("killing rpclib process {}", port_);
    logger_->debug("calling child.terminate()");
    if (contained_)
    {
        child_.terminate();
    }
    else
    {
        group_.terminate();
    }

    // To avoid a zombie process
    logger_->debug("calling child.wait()");
    child_.wait();

    // Although the server process has been killed, connecting to the port it
    // was listening on may still be possible (without getting a ECONNREFUSED),
    // but an RPC call won't get a response.

    logger_->info("rpclib server process killed");
    child_ = boost::process::child();
}

// Performs a synchronous RPC call (returning a response)
//
// Timeout given in milliseconds; a negative value means no timeout, to be used
// for resolve_sync() only, which returns after the resolution has finished.
//
// The implementation is based on rpc::client::call(), invoking
// rpc::client::async_call(). The difference is that we have a timeout that
// can differ between calls, and the rpclib library applies the same timeout
// to all calls (and also to a connection request).
template<typename... Params>
RPCLIB_MSGPACK::object_handle
rpclib_client_impl::do_rpc_call(
    std::string const& func_name, int timeout, Params&&... params)
{
    std::future<RPCLIB_MSGPACK::object_handle> fut;
    try
    {
        fut = rpc_client_->async_call(
            func_name, std::forward<Params>(params)...);
    }
    catch (rpc::rpc_error& e)
    {
        logger_->error(
            "do_rpc_call({}) caught {} in async_call: {}",
            func_name,
            e.what(),
            get_message(e));
        throw remote_error(e.what(), get_message(e), is_retryable(e));
    }
    if (timeout > 0)
    {
        auto status = fut.wait_for(std::chrono::milliseconds{timeout});
        if (status == std::future_status::timeout)
        {
            std::string msg{fmt::format(
                "do_rpc_call: timeout ({}ms) for {}", timeout, func_name)};
            logger_->error(msg);
            throw remote_error{msg};
        }
    }
    try
    {
        return fut.get();
    }
    catch (rpc::rpc_error& e)
    {
        logger_->error(
            "do_rpc_call({}) caught {} in fut.get(): {}",
            func_name,
            e.what(),
            get_message(e));
        throw remote_error(e.what(), get_message(e), is_retryable(e));
    }
}

// Performs an asynchronous RPC call (not expecting a response)
template<typename... Params>
void
rpclib_client_impl::do_rpc_async_call(
    std::string const& func_name, Params&&... params)
{
    try
    {
        rpc_client_->async_call(func_name, std::forward<Params>(params)...);
    }
    catch (rpc::rpc_error& e)
    {
        logger_->error(
            "do_rpc_async_call({}) caught {}: {}",
            func_name,
            e.what(),
            get_message(e));
        throw remote_error(e.what(), get_message(e), is_retryable(e));
    }
}

serialized_result
rpclib_client_impl::make_serialized_result(rpclib_response const& response)
{
    auto response_id = std::get<0>(response);
    auto record_lock_id_value = std::get<1>(response);
    blob value = std::get<2>(response);
    std::unique_ptr<deserialization_observer> observer;
    logger_->debug(
        "response_id {}, record_lock_id {}, value {}",
        response_id,
        record_lock_id_value,
        value);
    if (response_id != 0)
    {
        observer = std::make_unique<rpclib_deserialization_observer>(
            *this, response_id);
    }
    return serialized_result(
        value,
        std::move(observer),
        remote_cache_record_id{record_lock_id_value});
}

rpclib_deserialization_observer::rpclib_deserialization_observer(
    rpclib_client_impl& client, uint32_t pool_id)
    : client_{client}, pool_id_{pool_id}
{
}

void
rpclib_deserialization_observer::on_deserialized()
{
    client_.ack_response(pool_id_);
}

} // namespace cradle
