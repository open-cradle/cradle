#include <string>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

void
unique_hasher::finish()
{
    if (!finished_)
    {
        if (using_lambda_)
        {
            // TODO ensure the hash changes with lambdas
        }
        impl_.finish();
        finished_ = true;
    }
}

} // namespace cradle
