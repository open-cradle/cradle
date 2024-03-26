#include <cstdlib>
#include <mutex>
#include <vector>

#include <spdlog/cfg/helpers.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/fs/app_dirs.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

// Quoting the spdlog documentation:
//
//   If you want to have different loggers to write to the same output file all
//   of them must share the same sink. Otherwise you might get strange results.
//   ...
//   Each logger's sink have a formatter which formats the messages to its
//   destination.
//
// So we must have a single file sink with a single pattern, but each logger
// can have its own stdout sink with its own pattern. We will have two stdout
// sinks: one for the main "cradle" logger, one for all other loggers.

static spdlog::sink_ptr shared_file_sink;
static spdlog::sink_ptr main_stdout_sink;
static spdlog::sink_ptr other_stdout_sink;
static std::string level_spec;
static bool ignore_env_setting;

static auto
create_file_sink()
{
    auto log_path = get_user_logs_dir(none, "cradle") / "log";
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path.string(), 262144, 2);
    sink->set_pattern("[%H:%M:%S:%e] %L [thread %t] [%n] %v");
    return sink;
}

static auto
create_stdout_sink()
{
#ifdef _WIN32
    auto sink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
#else
    auto sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
#endif
    std::string pattern{"[%H:%M:%S:%e] %L [thread %t] [%n] %v"};
    sink->set_pattern(pattern);
    return sink;
}

// Load the levels for all existing loggers (no effect on loggers that have
// not yet been created)
static void
load_levels()
{
    std::string spec_to_use{level_spec};
    if (!ignore_env_setting)
    {
        // E.g. export SPDLOG_LEVEL=debug
        //      export SPDLOG_LEVEL=info,rpclib_server=debug
        auto env_var = std::getenv("SPDLOG_LEVEL");
        if (env_var != nullptr)
        {
            spec_to_use = env_var;
        }
    }
    spdlog::cfg::helpers::load_levels(spec_to_use);
}

void
initialize_logging(
    std::string const& level_spec_arg, bool ignore_env_setting_arg)
{
    shared_file_sink = create_file_sink();
    main_stdout_sink = create_stdout_sink();
    other_stdout_sink = create_stdout_sink();
    level_spec = level_spec_arg;
    ignore_env_setting = ignore_env_setting_arg;
    create_logger("cradle");
}

std::shared_ptr<spdlog::logger>
create_logger(std::string const& name)
{
    bool is_main = name == "cradle";
    auto stdout_sink{is_main ? main_stdout_sink : other_stdout_sink};
    std::vector<spdlog::sink_ptr> sinks{shared_file_sink, stdout_sink};
    auto logger{
        std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks))};
    spdlog::register_logger(logger);
    load_levels();
    return logger;
}

static std::mutex ensure_logger_mutex;

std::shared_ptr<spdlog::logger>
ensure_logger(std::string const& name)
{
    auto logger = spdlog::get(name);
    if (logger)
    {
        // The usual path, with the lowest overhead.
        // (Although spdlog::get() also involves a mutex lock.)
        return logger;
    }
    std::scoped_lock lock{ensure_logger_mutex};
    logger = spdlog::get(name);
    if (logger)
    {
        return logger;
    }
    return create_logger(name);
}

} // namespace cradle
