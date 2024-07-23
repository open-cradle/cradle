#include <stdexcept>
#include <string>

#include <fmt/format.h>
#include <msgpack.hpp>

#include <boost/program_options.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_dump.h>
#include <cradle/inner/requests/types.h>
#include <cradle/inner/utilities/git.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/rpclib/common/config.h>
#include <cradle/version_info.h>

// Command-line application interacting with an rpclib server.

using namespace cradle;

struct cli_options
{
    std::string log_level{"info"};
    bool ignore_env_log_level{false};
    rpclib_port_t port{RPCLIB_PORT_PRODUCTION};
    int remote_id;
};

static cli_options
parse_options(int argc, char const* const* argv)
{
    namespace po = boost::program_options;
    auto port_help{
        fmt::format("port number (default {})", RPCLIB_PORT_PRODUCTION)};

    // clang-format off
    po::options_description desc("Options");
    desc.add_options()
        ("help",
            "show help message")
        ("version",
            "show version information")
        ("log-level", po::value<std::string>(),
            "logging level (SPDLOG_LEVEL format)")
        ("port", po::value<rpclib_port_t>(),
            port_help.c_str())
        ("id", po::value<int>(),
            "remote id");
    // clang-format on

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << fmt::format("Usage: {} [OPTION]...\n", argv[0]);
        std::cout << "Interact with a CRADLE server.\n\n";
        std::cout << desc;
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
        options.log_level = vm["log-level"].as<std::string>();
        options.ignore_env_log_level = true;
    }
    if (vm.count("port"))
    {
        options.port = vm["port"].as<rpclib_port_t>();
    }
    options.remote_id = vm["id"].as<int>();

    return options;
}

static service_config_map
create_config_map(cli_options const& options)
{
    service_config_map config_map;
    // TODO don't start rpclib_server if not yet running
    config_map[rpclib_config_keys::PORT_NUMBER] = options.port;
    return config_map;
}

static void
run_cli(cli_options const& options)
{
    std::string prefix{"cli "};
    initialize_logging(
        options.log_level, options.ignore_env_log_level, std::move(prefix));
    auto my_logger = create_logger("cli");

    service_config config{create_config_map(options)};

    rpclib_client client{config, nullptr, my_logger};

    auto remote_id = options.remote_id;
    auto status = client.get_async_status(remote_id);
    fmt::print("id {}: status {}\n", remote_id, to_string(status));

    auto sub_specs = client.get_sub_contexts(remote_id);
    auto num_sub_specs = sub_specs.size();
    for (decltype(num_sub_specs) i = 0; i < num_sub_specs; ++i)
    {
        auto const& spec = sub_specs[i];
        auto const& [spec_id, is_req] = spec;
        fmt::print(
            "sub [{}]: id {} ({})\n", i, spec_id, is_req ? "REQ" : "VAL");
    }
    if (status == async_status::ERROR)
    {
        fmt::print("error: {}\n", client.get_async_error_message(remote_id));
    }
    try
    {
        auto essentials = client.get_essentials(remote_id);
        fmt::print("uuid {}\n", essentials.uuid_str);
        if (essentials.title)
        {
            fmt::print("title {}\n", *essentials.title);
        }
    }
    catch (remote_error const& e)
    {
        fmt::print("get_essentials: caught {}\n", e.what());
    }
    if (status == async_status::FINISHED)
    {
        try
        {
            auto result = client.get_async_response(remote_id);
            auto x = result.value();
            msgpack::object_handle oh = msgpack::unpack(
                reinterpret_cast<char const*>(x.data()), x.size());
            msgpack::object obj = oh.get();
            fmt::print("result: ");
            dump_msgpack_object(obj);
        }
        catch (remote_error const&)
        {
            // No root ctx probably
            // TODO logger shouldn't report error. Default FATAL only?
        }
    }
}

int
main(int argc, char const* const* argv)
try
{
    auto options = parse_options(argc, argv);
    run_cli(options);
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
