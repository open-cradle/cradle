#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/typing/core/dynamic.h>
#include <cradle/typing/core/type_interfaces.h>
#include <cradle/typing/core/unique_hash.h>
#include <cradle/websocket/calculations.h>
#include <cradle/websocket/sample_lambdas.h>
#include <cradle/websocket/types.hpp>

namespace cradle {

CRADLE_DEFINE_EXCEPTION(unknown_lambda_name)
CRADLE_DEFINE_ERROR_INFO(string, bad_lambda_name)

static dynamic
sleeper(dynamic_array const& args, tasklet_tracker* tasklet)
{
    double value;
    from_dynamic(&value, args[0]);
    if (tasklet)
    {
        std::ostringstream os;
        os << "Sleep for " << value << " seconds";
        tasklet->log(os.str());
    }
    std::chrono::duration<double> duration{value};
    std::this_thread::sleep_for(duration);

    dynamic result;
    to_dynamic(&result, 42);
    return result;
}

void
find_sample_lambda(lambda_function* x, dynamic const& v)
{
    std::string lambda_name;
    from_dynamic(&lambda_name, v);
    if (lambda_name == "sleep")
    {
        *x = make_function(sleeper);
    }
    else
    {
        CRADLE_THROW(
            unknown_lambda_name() << bad_lambda_name_info(lambda_name));
    }
}

} // namespace cradle
