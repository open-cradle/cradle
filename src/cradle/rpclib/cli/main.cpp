#include <map>
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
    std::string command;
    std::string log_level{"critical"};
    bool ignore_env_log_level{false};
    rpclib_port_t port{RPCLIB_PORT_PRODUCTION};
    int remote_id{-1};
};

static service_config_map
create_config_map(cli_options const& options)
{
    service_config_map config_map;
    config_map[rpclib_config_keys::EXPECT_SERVER] = true;
    config_map[rpclib_config_keys::PORT_NUMBER] = options.port;
    return config_map;
}

static void
cmd_show(cli_options const& options)
{
    auto const remote_id = options.remote_id;
    if (remote_id < 0)
    {
        // TODO cli_error
        throw std::runtime_error{"missing --id"};
    }
    std::string prefix{"cli "};
    initialize_logging(
        options.log_level, options.ignore_env_log_level, std::move(prefix));
    auto my_logger = create_logger("cli");

    service_config config{create_config_map(options)};

    rpclib_client client{config, nullptr, my_logger};

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
        my_logger->warn("No essentials for id {}: {}", remote_id, e.what());
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
        catch (remote_error const& e)
        {
            // No root ctx probably
            my_logger->warn("No result for id {}: {}", remote_id, e.what());
        }
    }
}

class my_parser
{
 public:
    my_parser(int argc, char const* const* argv);

    void
    parse();

    void
    run_cmd();

 private:
    int const argc_;
    char const* const* argv_;

    // These set in the constructor, via define_visible() / define_hidden()
    std::string port_help_;
    boost::program_options::options_description visible_{"Options"};
    boost::program_options::options_description hidden_;

    // Set in make_vm()
    boost::program_options::variables_map vm_;
    // Set in make_options()
    cli_options options_;

    void
    define_visible();
    void
    define_hidden();

    void
    show_help();
    void
    bad_command_line(std::string const& reason);

    void
    make_vm();
    void
    make_options();
};

my_parser::my_parser(int argc, char const* const* argv)
    : argc_{argc}, argv_{argv}
{
    define_visible();
    define_hidden();
}

void
my_parser::define_visible()
{
    namespace po = boost::program_options;
    port_help_
        = fmt::format("port number (default {})", RPCLIB_PORT_PRODUCTION);
    // clang-format off
    visible_.add_options()
        ("help",
            "show help message")
        ("version",
            "show version information")
        ("log-level", po::value<std::string>(),
            "logging level (SPDLOG_LEVEL format)")
        ("port", po::value<rpclib_port_t>(),
            port_help_.c_str())
        ("id", po::value<int>(),
            "remote id");
    // clang-format on
}

void
my_parser::define_hidden()
{
    namespace po = boost::program_options;
    // clang-format off
    hidden_.add_options()
        ("hidden-cmd", po::value<std::string>(),
            "command");
    // clang-format on
}

void
my_parser::show_help()
{
    fmt::print("Usage: {} [CMD] [OPTION]...\n", argv_[0]);
    std::cout << "Interact with a CRADLE server.\n\n";
    std::cout << "Commands:\n";
    std::cout << "  show                  show status of remote context "
                 "identified by --id\n";
    std::cout << "\n";
    std::cout << visible_;
    std::cout << "\n";
    std::cout << "Examples:\n";
    fmt::print("  {} show --port 8096 --id 1\n", argv_[0]);
}

void
my_parser::bad_command_line(std::string const& reason)
{
    std::cerr << reason << "\n\n";
    show_help();
    std::exit(1);
}

void
my_parser::parse()
{
    make_vm();
    make_options();
}

void
my_parser::make_vm()
{
    namespace po = boost::program_options;

    po::options_description all_options;
    all_options.add(visible_).add(hidden_);

    po::positional_options_description positional;
    positional.add("hidden-cmd", -1);

    po::store(
        po::command_line_parser(argc_, argv_)
            .options(all_options)
            .positional(positional)
            .run(),
        vm_);
    notify(vm_);

    if (vm_.count("help"))
    {
        show_help();
        std::exit(0);
    }

    if (vm_.count("version"))
    {
        show_version_info(version_info);
        std::exit(0);
    }
}

void
my_parser::make_options()
{
    if (vm_.count("hidden-cmd"))
    {
        options_.command = vm_["hidden-cmd"].as<std::string>();
    }
    else
    {
        bad_command_line("Missing command");
    }
    if (vm_.count("log-level"))
    {
        options_.log_level = vm_["log-level"].as<std::string>();
        options_.ignore_env_log_level = true;
    }
    if (vm_.count("port"))
    {
        options_.port = vm_["port"].as<rpclib_port_t>();
    }
    if (vm_.count("id"))
    {
        options_.remote_id = vm_["id"].as<int>();
    }
}

void
my_parser::run_cmd()
{
    using cmd_func_ptr = void (*)(cli_options const& options);
    std::map<std::string, cmd_func_ptr> const func_map = {{"show", cmd_show}};
    auto it = func_map.find(options_.command);
    if (it == func_map.end())
    {
        bad_command_line(fmt::format("Unknown command {}", options_.command));
    }
    it->second(options_);
}

int
main(int argc, char const* const* argv)
try
{
    my_parser parser{argc, argv};
    parser.parse();
    parser.run_cmd();
    return 0;
}
catch (std::exception& e)
{
    std::cerr << "main() caught " << e.what() << "\n";
    return 1;
}
catch (...)
{
    std::cerr << "unknown error\n";
    return 1;
}
