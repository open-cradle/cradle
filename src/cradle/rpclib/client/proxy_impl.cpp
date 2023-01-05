#include <chrono>
#include <typeinfo>

#include <cppcoro/schedule_on.hpp>
#include <rpc/client.h>
#include <rpc/rpc_error.h>

#include <cradle/deploy_dir.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_adaptors_rpclib.h>
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

// Performs an RPC call.
// rpc::client::call is blocking. Internally it calls async_call() which
// returns a future.
template<typename... Args>
auto
do_rpc_call(rpc::client& rpc_client, Args... args)
{
    try
    {
        return rpc_client.call(std::move(args)...);
    }
    catch (rpc::rpc_error& e)
    {
        throw rpclib_error(e.what(), get_message(e));
    }
}

cppcoro::task<blob>
call_resolve_sync(
    rpc::client& rpc_client, std::string domain_name, std::string seri_req)
{
    co_return do_rpc_call(
        rpc_client,
        "resolve_sync",
        std::move(domain_name),
        std::move(seri_req))
        .as<blob>();
}

} // namespace

rpclib_client_impl::rpclib_client_impl(service_config const& config)
    : port_(RPCLIB_PORT),
      thread_pool_{cppcoro::static_thread_pool(
          static_cast<uint32_t>(config.get_number_or_default(
              rpclib_config_keys::CLIENT_CONCURRENCY, 22)))}
{
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

cppcoro::task<blob>
rpclib_client_impl::resolve_request(
    remote_context_intf& ctx, std::string seri_req)
{
    std::string domain_name{ctx.domain_name()};
    logger_->debug("request on {}: {}", domain_name, seri_req);
    blob result = co_await cppcoro::schedule_on(
        thread_pool_,
        call_resolve_sync(
            *rpc_client_, std::move(domain_name), std::move(seri_req)));
    logger_->debug("response {}", result);
    co_return result;
}

void
rpclib_client_impl::mock_http(std::string const& response_body)
{
    logger_->debug("mock_http start");
    do_rpc_call(*rpc_client_, "mock_http", response_body);
    logger_->debug("mock_http finished");
}

// Note is blocking
int
rpclib_client_impl::ping()
{
    logger_->debug("ping");
    int result = do_rpc_call(*rpc_client_, "ping").as<int>();
    logger_->debug("pong {}", result);
    return result;
}

bool
rpclib_client_impl::server_is_running()
{
    logger_->info("test whether rpclib server is running");
    try
    {
        rpc_client_ = std::make_unique<rpc::client>(localhost_, port_);
        ping();
    }
    catch (rpc::system_error& e)
    {
        // rpclib sets the error code, not what
        logger_->info(
            "rpclib server is not running (code {})", e.code().value());
        return false;
    }
    logger_->info("received pong: rpclib server is running");
    return true;
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
            throw rpclib_error("could not start rpclib_server", "timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void
rpclib_client_impl::start_server()
{
    if (server_is_running())
    {
        return;
    }
    std::string path{get_rpclib_server_path()};
    logger_->info("starting {}", path);
    // child constructor takes Args&& which is rather inflexible.
    // Maybe boost::process is not the best choice after all.
    auto child = boost::process::child(path, "--log-level", "warn", group_);
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
    logger_->info("killing rpclib process");
    logger_->debug("calling group.terminate()");
    group_.terminate();

    // To avoid a zombie process
    logger_->debug("calling child.wait()");
    child_.wait();

    logger_->info("rpclib server process killed");
    child_ = boost::process::child();
}

} // namespace cradle
