#include <iostream>
#include <map>

#include <fmt/format.h>

#include <cradle/rpclib/cli/cmd_cancel.h>
#include <cradle/rpclib/cli/cmd_show.h>
#include <cradle/rpclib/cli/parser.h>
#include <cradle/rpclib/cli/types.h>

// Command-line application interacting with an rpclib server.

namespace cradle {

void
run_cmd(cli_options const& options)
{
    using cmd_func_ptr = void (*)(cli_options const& options);
    std::map<std::string, cmd_func_ptr> const func_map{
        {"cancel", cmd_cancel}, {"show", cmd_show}};
    auto it = func_map.find(options.command);
    if (it == func_map.end())
    {
        throw command_line_error{
            fmt::format("unknown command `{}`", options.command)};
    }
    it->second(options);
}

} // namespace cradle

int
main(int argc, char const* const* argv)
{
    cradle::cli_parser parser{argc, argv};
    bool suggest_log_level{false};
    try
    {
        auto const& options = parser.parse();
        suggest_log_level = !options.log_level_set;
        run_cmd(options);
    }
    catch (boost::program_options::error const& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Type `" << argv[0] << " --help` for help\n";
        return 1;
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        if (suggest_log_level)
        {
            std::cerr << "Consider setting --log-level for details\n";
        }
        return 1;
    }
    return 0;
}
