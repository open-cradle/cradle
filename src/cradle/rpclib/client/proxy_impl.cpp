#include <typeinfo>

#include <boost/filesystem.hpp>
#include <fmt/format.h>
#include <rpc/client.h>
#include <rpc/rpc_error.h>

#include <cradle/deploy_dir.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_adaptors_rpclib.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>

#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>
#include <cradle/rpclib/common/common.h>

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

} // namespace

rpclib_client_impl::rpclib_client_impl(service_config const& config)
{
    testing_ = config.get_bool_or_default(generic_config_keys::TESTING, false);
    deploy_dir_ = config.get_optional_string(generic_config_keys::DEPLOY_DIR);
    port_ = testing_ ? RPCLIB_PORT_TESTING : RPCLIB_PORT_PRODUCTION;
    secondary_cache_factory_ = config.get_mandatory_string(
        inner_config_keys::SECONDARY_CACHE_FACTORY);
    if (!logger_)
    {
        logger_ = create_logger("rpclib_client");
    }
    start_server();
}

rpclib_client_impl::~rpclib_client_impl()
{
    stop_server();
}

serialized_result
rpclib_client_impl::resolve_sync(std::string domain_name, std::string seri_req)
{
    logger_->debug("resolve_sync");
    auto response
        = do_rpc_call(
              "resolve_sync", std::move(domain_name), std::move(seri_req))
              .as<rpclib_response>();
    return make_serialized_result(std::move(response));
}

async_id
rpclib_client_impl::submit_async(
    remote_context_intf& ctx, std::string domain_name, std::string seri_req)
{
    logger_->debug("submit_async");
    auto aid = do_rpc_call(
                   "submit_async", std::move(domain_name), std::move(seri_req))
                   .as<async_id>();
    logger_->debug("submit_async -> {}", aid);
    return aid;
}

remote_context_spec_list
rpclib_client_impl::get_sub_contexts(async_id aid)
{
    logger_->debug("get_sub_contexts {}", aid);
    auto result
        = do_rpc_call("get_sub_contexts", aid).as<remote_context_spec_list>();
    logger_->debug("get_sub_contexts {} -> {} sub(s)", aid, result.size());
    return result;
}

async_status
rpclib_client_impl::get_async_status(async_id aid)
{
    logger_->debug("get_async_status {}", aid);
    auto status_value = do_rpc_call("get_async_status", aid).as<int>();
    async_status status{status_value};
    logger_->debug("async_status for {}: {}", aid, status);
    return status;
}

std::string
rpclib_client_impl::get_async_error_message(async_id aid)
{
    logger_->debug("get_async_error_message {}", aid);
    auto errmsg
        = do_rpc_call("get_async_error_message", aid).as<std::string>();
    logger_->debug("async_error_message for {}: {}", aid, errmsg);
    return errmsg;
}

serialized_result
rpclib_client_impl::get_async_response(async_id root_aid)
{
    logger_->debug("get_async_response {}", root_aid);
    auto response
        = do_rpc_call("get_async_response", root_aid).as<rpclib_response>();
    return make_serialized_result(std::move(response));
}

void
rpclib_client_impl::request_cancellation(async_id aid)
{
    logger_->debug("request_cancellation {}", aid);
    do_rpc_call("request_cancellation", aid);
    logger_->debug("request_cancellation done");
}

void
rpclib_client_impl::finish_async(async_id root_aid)
{
    logger_->debug("finish_async {}", root_aid);
    do_rpc_call("finish_async", root_aid);
    logger_->debug("finish_async done");
}

void
rpclib_client_impl::mock_http(std::string const& response_body)
{
    logger_->debug("mock_http start");
    do_rpc_call("mock_http", response_body);
    logger_->debug("mock_http finished");
}

// Note is blocking
std::string
rpclib_client_impl::ping()
{
    logger_->debug("ping");
    std::string result = do_rpc_call("ping").as<std::string>();
    logger_->debug("pong {}", result);
    return result;
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
    logger_->info("test whether rpclib server is running");
    std::string server_git_version;
    try
    {
        rpc_client_ = std::make_unique<rpc::client>(localhost_, port_);
        server_git_version = ping();
    }
    catch (rpc::system_error& e)
    {
        // rpclib sets the error code, not what
        logger_->info(
            "rpclib server is not running (code {})", e.code().value());
        return false;
    }
    logger_->info(
        "received pong {}: rpclib server is running", server_git_version);
    // Detect a rogue rpclib server instance (TODO finetune)
    verify_git_version(server_git_version);
    // rpc_client_->set_timeout(30);
    return true;
}

void
rpclib_client_impl::verify_git_version(std::string const& server_git_version)
{
    std::string client_git_version{request_uuid::get_git_version()};
    if (server_git_version != client_git_version)
    {
        auto msg{fmt::format(
            "rpclib server has {}, client has {}",
            server_git_version,
            client_git_version)};
        logger_->error(msg);
        throw remote_error("code version mismatch", msg);
    }
}

void
rpclib_client_impl::wait_until_server_running()
{
    int attempts_left = 10;
    while (!server_is_running())
    {
        attempts_left -= 1;
        if (attempts_left == 0)
        {
            throw remote_error("could not start rpclib_server", "timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    child_args.push_back("--secondary-cache");
    child_args.push_back(secondary_cache_factory_);
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
    auto child = bp::child(bp::exe = path, bp::args = child_args, group_);
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
    if (!testing_)
    {
        logger_->info("keep rpclib process running");
        return;
    }
    logger_->info("killing rpclib process");
    logger_->debug("calling group.terminate()");
    group_.terminate();

    // To avoid a zombie process
    logger_->debug("calling child.wait()");
    child_.wait();

    logger_->info("rpclib server process killed");
    child_ = boost::process::child();
}

// Performs a synchronous RPC call.
// rpc::client::call is blocking. Internally it calls async_call() which
// returns a future.
template<typename... Args>
RPCLIB_MSGPACK::object_handle
rpclib_client_impl::do_rpc_call(std::string const& func_name, Args... args)
{
    try
    {
        return rpc_client_->call(func_name, std::move(args)...);
    }
    catch (rpc::rpc_error& e)
    {
        logger_->error(
            "do_rpc_call({}) caught {}: {}",
            func_name,
            e.what(),
            get_message(e));
        throw remote_error(e.what(), get_message(e));
    }
}

// Performs an asynchronous RPC call.
template<typename... Args>
void
rpclib_client_impl::do_rpc_async_call(
    std::string const& func_name, Args... args)
{
    try
    {
        rpc_client_->async_call(func_name, std::move(args)...);
    }
    catch (rpc::rpc_error& e)
    {
        logger_->error(
            "do_rpc_async_call({}) caught {}: {}",
            func_name,
            e.what(),
            get_message(e));
        throw remote_error(e.what(), get_message(e));
    }
}

serialized_result
rpclib_client_impl::make_serialized_result(rpclib_response const& response)
{
    auto response_id = std::get<0>(response);
    blob value = std::get<1>(response);
    std::unique_ptr<deserialization_observer> observer;
    logger_->debug("response_id {}, value {}", response_id, value);
    if (response_id != 0)
    {
        observer = std::make_unique<rpclib_deserialization_observer>(
            *this, response_id);
    }
    return serialized_result(value, std::move(observer));
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
