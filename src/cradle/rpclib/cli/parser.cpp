#include <cstdlib>
#include <iostream>

#include <fmt/format.h>

#include <cradle/rpclib/cli/parser.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/version_info.h>

namespace cradle {

cli_parser::cli_parser(int argc, char const* const* argv)
    : argc_{argc}, argv_{argv}
{
    define_visible();
    define_hidden();
}

void
cli_parser::define_visible()
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
cli_parser::define_hidden()
{
    namespace po = boost::program_options;
    // clang-format off
    hidden_.add_options()
        ("hidden-cmd", po::value<std::string>(),
            "command");
    // clang-format on
}

void
cli_parser::show_help()
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

cli_options const&
cli_parser::parse()
{
    make_vm();
    make_options();
    return options_;
}

void
cli_parser::make_vm()
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
cli_parser::make_options()
{
    if (vm_.count("hidden-cmd"))
    {
        options_.command = vm_["hidden-cmd"].as<std::string>();
    }
    else
    {
        throw command_line_error{"missing command"};
    }
    if (vm_.count("log-level"))
    {
        options_.log_level = vm_["log-level"].as<std::string>();
        options_.log_level_set = true;
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

} // namespace cradle
