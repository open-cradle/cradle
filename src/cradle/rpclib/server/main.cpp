#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/program_options.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>
#include <rpc/server.h>
#include <rpc/this_server.h>
#include <spdlog/spdlog.h>

// Ensure to #include only from the msgpack inside rpclib
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/encodings/msgpack_adaptors_rpclib.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/utilities/git.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/secondary_cache/all_plugins.h>
#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/simple/simple_storage.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/rpclib/common/config.h>
#include <cradle/rpclib/server/handlers.h>
#include <cradle/thinknode/domain_factory.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/version_info.h>

// The rpclib server interprets RPC messages sent by an rpclib client;
// the main messages instruct to resolve a request.
//
// The server runs in production, testing or contained mode. The main
// difference between production and testing is the port on which the server
// listens. In contained mode, a request encodes a single function call, and
// has no subrequests. In addition, the request is resolved without any form of
// caching; caching can still happen in the client, which could be another
// rpclib server (running in non-contained mode).

using namespace cradle;

struct cli_options
{
    std::string log_level{"info"};
    bool ignore_env_log_level{false};
    bool testing{false};
    bool contained{false};
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
        ("contained",
            "set contained mode")
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
        show_version_info(version_info);
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
    if (vm.count("contained"))
    {
        options.contained = true;
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
    std::string cache_dir{
        options.testing ? "server_cache_testing" : "server_cache_production"};
    service_config_map config_map;
    if (options.testing)
    {
        config_map[generic_config_keys::TESTING] = true;
    }
    if (options.contained)
    {
        // Won't create any caches in contained mode
        config_map[rpclib_config_keys::CONTAINED] = true;
    }
    else
    {
        config_map[inner_config_keys::SECONDARY_CACHE_FACTORY]
            = options.secondary_cache;
        config_map[local_disk_cache_config_keys::DIRECTORY] = cache_dir;
    }
    config_map[blob_cache_config_keys::DIRECTORY] = cache_dir;
    config_map[http_cache_config_keys::PORT] = 9090U;
    config_map[generic_config_keys::DEPLOY_DIR]
        = boost::dll::program_location().parent_path().string();
    return config_map;
}

static void
run_server(cli_options const& options)
{
    std::string prefix{"server "};
    if (options.contained)
    {
        prefix = fmt::format("port {} ", options.port);
    }
    initialize_logging(
        options.log_level, options.ignore_env_log_level, std::move(prefix));
    auto my_logger = create_logger("rpclib_server");

    service_config config{create_config_map(options)};
    service_core service{config};
    if (!options.contained)
    {
        service.set_secondary_cache(create_secondary_storage(service));
    }
    service.set_requests_storage(
        std::make_unique<simple_blob_storage>("simple"));
    service.ensure_async_db();
    service.register_domain(create_testing_domain(service));
    service.register_domain(create_thinknode_domain(service));
    rpclib_handler_context hctx{config, service, *my_logger};

    rpc::server srv("127.0.0.1", options.port);
    my_logger->info("listening on port {}", srv.port());

    // TODO enable introspection only on demand
    introspection_set_capturing_enabled(service.the_tasklet_admin(), true);

    // TODO we need a session concept and a "start session" / "register"
    // (notification) message
    if (options.testing)
    {
        // No mocking in production server
        srv.bind("mock_http", [&](std::string body) {
            return handle_mock_http(hctx, std::move(body));
        });
    }
    srv.bind("ack_response", [&](int response_id) {
        return handle_ack_response(hctx, response_id);
    });
    srv.bind("ping", []() { return RPCLIB_PROTOCOL; });

    srv.bind(
        "store_request",
        [&](std::string storage_name, std::string key, std::string seri_req) {
            return handle_store_request(
                hctx,
                std::move(storage_name),
                std::move(key),
                std::move(seri_req));
        });
    srv.bind(
        "resolve_sync", [&](std::string config_json, std::string seri_req) {
            return handle_resolve_sync(
                hctx, std::move(config_json), std::move(seri_req));
        });
    srv.bind(
        "submit_async", [&](std::string config_json, std::string seri_req) {
            return handle_submit_async(
                hctx, std::move(config_json), std::move(seri_req));
        });
    srv.bind(
        "submit_stored",
        [&](std::string config_json,
            std::string storage_name,
            std::string key) {
            return handle_submit_stored(
                hctx,
                std::move(config_json),
                std::move(storage_name),
                std::move(key));
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
    srv.bind("get_tasklet_infos", [&](bool include_finished) {
        return handle_get_tasklet_infos(hctx, include_finished);
    });
    srv.bind(
        "load_shared_library",
        [&](std::string dir_path, std::string dll_name) {
            handle_load_shared_library(
                hctx, std::move(dir_path), std::move(dll_name));
        });
    srv.bind("unload_shared_library", [&](std::string dll_name) {
        handle_unload_shared_library(hctx, std::move(dll_name));
    });
    srv.bind("clear_unused_mem_cache_entries", [&]() {
        handle_clear_unused_mem_cache_entries(hctx);
    });
    srv.bind(
        "release_cache_record_lock",
        [&](remote_cache_record_id::value_type record_id_value) {
            handle_release_cache_record_lock(
                hctx, remote_cache_record_id{record_id_value});
        });
    srv.bind("get_num_contained_calls", [&]() {
        return handle_get_num_contained_calls(hctx);
    });
    srv.bind("get_essentials", [&](async_id aid) {
        return handle_get_essentials(hctx, aid);
    });

    auto num_threads{hctx.handler_pool_size()};
    assert(num_threads >= 2);
    // Create a pool with all handler threads except one
    srv.async_run(num_threads - 1);
    // One additional handler on the current thread
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
