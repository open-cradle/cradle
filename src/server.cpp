#include <cradle/websocket/server.h>

#include <boost/program_options.hpp>

#include <cradle/inner/fs/app_dirs.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/service/config_map_file.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/secondary_cache/all_plugins.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/version_info.h>

using namespace cradle;

static void
show_version_info()
{
    if (is_tagged_version(version_info))
    {
        std::cout << "CRADLE " << version_info.tag << "\n";
    }
    else
    {
        std::cout << "CRADLE (unreleased version - "
                  << version_info.commit_object_name << ", "
                  << version_info.commits_since_tag << " commits ahead of "
                  << version_info.tag;
        if (version_info.dirty)
            std::cout << ", with local modifications";
        std::cout << ")\n";
    }
}

int
main(int argc, char const* const* argv)
try
{
    namespace po = boost::program_options;

    po::options_description desc("Supported options");
    desc.add_options()("help", "show help message")(
        "version", "show version information")(
        "config-file",
        po::value<string>(),
        "specify the configuration file to use");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        show_version_info();
        std::cout << desc;
        return 0;
    }

    if (vm.count("version"))
    {
        show_version_info();
        return 0;
    }

    optional<file_path> config_path;
    if (vm.count("config-file"))
    {
        config_path = vm["config-file"].as<string>();
    }
    else
    {
        config_path = search_in_path(
            get_config_search_path(none, "cradle"), "config.json");
    }

    service_config_map config_map;
    if (config_path)
        config_map = read_config_map_from_file(*config_path);
    initialize_logging();

    activate_all_secondary_storage_plugins();

    // Default values for mandatory options
    config_map.try_emplace(
        inner_config_keys::SECONDARY_CACHE_FACTORY,
        local_disk_cache_config_values::PLUGIN_NAME);

    service_config config{config_map};
    websocket_server server(config);
    server.listen();
    server.run();
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
