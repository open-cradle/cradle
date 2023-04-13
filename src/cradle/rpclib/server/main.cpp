#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>
#include <rpc/server.h>
#include <rpc/this_server.h>
#include <spdlog/spdlog.h>

// Ensure to #include only from the msgpack inside rpclib
#include <cradle/inner/encodings/msgpack_adaptors_rpclib.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/all/all_domains.h>
#include <cradle/plugins/secondary_cache/all_plugins.h>
#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/rpclib/server/handlers.h>
#include <cradle/typing/service/core.h>

using namespace cradle;

// TODO maybe this should be a config_map?
struct cli_options
{
    std::string log_level{"info"};
    bool ignore_env_log_level{false};
    bool testing{false};
    rpclib_port_t port{RPCLIB_PORT_PRODUCTION};
    std::string secondary_cache{local_disk_cache_config_values::PLUGIN_NAME};
};

static cli_options
parse_options(int argc, char const* const* argv)
{
    namespace po = boost::program_options;

    // clang-format off
    po::options_description desc("Options");
    desc.add_options()
        ("help",
            "show help message")
        ("version",
            "show version information")
        ("log-level", po::value<std::string>(),
            "logging level (SPDLOG_LEVEL format)")
        ("testing",
            "set testing environment")
        ("port", po::value<rpclib_port_t>(),
            "port number")
        ("secondary-cache", po::value<std::string>(),
            "secondary cache plugin");
    // clang-format on

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << fmt::format("Usage: {} [OPTION]...\n", argv[0]);
        std::cout << "Interprets CRADLE RPC commands.\n\n";
        std::cout << desc;
        auto names{get_secondary_storage_plugin_names()};
        std::string joined_names("(none)");
        if (names.size() > 0)
        {
            joined_names = boost::algorithm::join(names, ", ");
        }
        std::cout << "\nAvailable secondary cache(s): " << joined_names
                  << "\n";
        std::exit(0);
    }

    if (vm.count("version"))
    {
        std::cout << "TODO version\n";
        std::exit(0);
    }

    cli_options options;
    if (vm.count("log-level"))
    {
        options.log_level = vm["log-level"].as<string>();
        options.ignore_env_log_level = true;
    }
    if (vm.count("testing"))
    {
        options.testing = true;
        options.port = RPCLIB_PORT_TESTING;
    }
    if (vm.count("port"))
    {
        options.port = vm["port"].as<rpclib_port_t>();
    }
    if (vm.count("secondary-cache"))
    {
        options.secondary_cache = vm["secondary-cache"].as<string>();
    }

    return options;
}

// TODO load the config_map from somewhere
static service_config_map
create_config_map(cli_options const& options)
{
    service_config_map config_map;

    if (options.testing)
    {
        config_map[generic_config_keys::TESTING] = true;
    }
    config_map[inner_config_keys::SECONDARY_CACHE_FACTORY]
        = options.secondary_cache;
    std::string cache_dir{
        options.testing ? "server_cache_testing" : "server_cache_production"};
    config_map[local_disk_cache_config_keys::DIRECTORY] = cache_dir;
    config_map[blob_cache_config_keys::DIRECTORY] = cache_dir;
    config_map[http_cache_config_keys::PORT] = 9090U;

    return config_map;
}

static void
run_server(cli_options const& options)
{
    initialize_logging(options.log_level, options.ignore_env_log_level);
    auto my_logger = create_logger("rpclib_server");

    activate_all_secondary_storage_plugins();

    service_core service;
    service_config config{create_config_map(options)};
    service.initialize(config);
    service.ensure_async_db();
    rpclib_handler_context hctx{config, service, *my_logger};

    register_and_initialize_all_domains();

    rpc::server srv(options.port);
    my_logger->info("listening on port {}", srv.port());

    // TODO we need a session concept and a "start session" / "register"
    // (notification) message
    srv.bind(
        "resolve_sync",
        [&](std::string const& domain_name, std::string const& seri_req) {
            // TODO create origin tasklet somewhere
            return handle_resolve_sync(hctx, domain_name, seri_req);
        });
    if (options.testing)
    {
        // No mocking in production server
        srv.bind("mock_http", [&](std::string const& body) {
            return handle_mock_http(hctx, body);
        });
    }
    srv.bind("ack_response", [&](int response_id) {
        return handle_ack_response(hctx, response_id);
    });
    srv.bind("ping", []() { return request_uuid::get_git_version(); });

    srv.bind(
        "submit_async",
        [&](std::string const& domain_name, std::string const& seri_req) {
            return handle_submit_async(hctx, domain_name, seri_req);
        });
    srv.bind("get_sub_contexts", [&](async_id aid) {
        return handle_get_sub_contexts(hctx, aid);
    });
    srv.bind("get_async_status", [&](async_id aid) {
        return handle_get_async_status(hctx, aid);
    });
    srv.bind("get_async_error_message", [&](async_id aid) {
        return handle_get_async_error_message(hctx, aid);
    });
    srv.bind("get_async_response", [&](async_id root_aid) {
        return handle_get_async_response(hctx, root_aid);
    });
    srv.bind("request_cancellation", [&](async_id aid) {
        return handle_request_cancellation(hctx, aid);
    });
    srv.bind("finish_async", [&](async_id root_aid) {
        return handle_finish_async(hctx, root_aid);
    });

    srv.run();

    rpc::this_server().stop();
}

int
main(int argc, char const* const* argv)
try
{
    auto options = parse_options(argc, argv);
    run_server(options);
    return 0;
}
catch (std::exception& e)
{
    std::cerr << e.what() << "\n";
    return 1;
}
catch (...)
{
    std::cerr << "unknown error\n";
    return 1;
}
