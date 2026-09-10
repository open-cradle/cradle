#include <cradle/inner/resolve/resolve_pool.h>

#include <algorithm>
#include <chrono>

#include <fmt/format.h>

#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_spec_builder.h>
#include <cradle/inner/pool/pool_intf.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/mutable_store_intf.h>

namespace cradle {

namespace {

// Inner-side built-in poll cadence bounds, in milliseconds. The configurable
// cadence lives in the plugins layer, which the inner branch must not depend
// on, so these built-in defaults apply here; configurable cadence is future
// work.
constexpr int poll_initial_interval_ms{1};
constexpr int poll_max_interval_ms{100};

// Read the content associated with a digest that has been established to
// exist; an absent digest is a genuine backend fault, not a miss.
cppcoro::task<blob>
get_required_content(cas_intf& cas, digest const& content_digest)
{
    auto opt_content = co_await cas.get(content_digest);
    if (!opt_content)
    {
        throw async_error{
            "pool result content missing from content-addressable store"};
    }
    co_return *opt_content;
}

} // namespace

cppcoro::task<job_record>
observe_until_terminal(local_context_intf& ctx, pool_intf& pool, job_id id)
{
    auto& store = pool.mutable_store();
    auto const key = job_record_key(id);
    int interval_ms{poll_initial_interval_ms};
    for (;;)
    {
        auto opt_value = co_await store.get(key);
        if (opt_value)
        {
            auto record = deserialize_job_record(*opt_value);
            if (is_terminal(record.status))
            {
                co_return record;
            }
        }
        co_await ctx.schedule_after(std::chrono::milliseconds(interval_ms));
        interval_ms
            = std::min((interval_ms + 1) * 3 / 2, poll_max_interval_ms);
    }
}

cppcoro::task<blob>
resolve_leaf_on_pool(
    local_context_intf& ctx,
    pool_intf& pool,
    provider_id provider,
    request_key key,
    context_id context,
    std::vector<blob> serialized_inputs,
    std::size_t input_size_boundary)
{
    // AC-hit short-circuit: when the pool's action cache already associates
    // the request key with a result digest, return that content without
    // submitting a job.
    if (auto opt_digest = co_await pool.ac().get(key))
    {
        co_return co_await get_required_content(pool.cas(), *opt_digest);
    }
    // No AC hit: build the job spec (classifying inputs into the pool's CAS),
    // submit it, and observe its record until terminal.
    auto spec = co_await build_job_spec(
        pool.cas(),
        std::move(provider),
        std::move(key),
        std::move(context),
        std::move(serialized_inputs),
        input_size_boundary);
    auto id = co_await pool.submit(std::move(spec));
    auto record = co_await observe_until_terminal(ctx, pool, id);
    switch (record.status)
    {
        case job_status::succeeded:
            co_return co_await get_required_content(
                pool.cas(), record.output_digest);
        case job_status::cancelled:
            throw async_cancelled{fmt::format("pool job {} cancelled", id)};
        case job_status::failed:
        default:
            throw async_error{record.error};
    }
}

} // namespace cradle
