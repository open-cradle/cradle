#ifndef CRADLE_RPCLIB_CLI_PARSER_H
#define CRADLE_RPCLIB_CLI_PARSER_H

#include <string>

#include <boost/program_options.hpp>

#include <cradle/rpclib/cli/types.h>

namespace cradle {

class cli_parser
{
 public:
    cli_parser(int argc, char const* const* argv);

    cli_options const&
    parse();

    void
    show_help();

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
    make_vm();
    void
    make_options();
};

} // namespace cradle

#endif
