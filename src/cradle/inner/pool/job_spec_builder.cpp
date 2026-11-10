#include <cradle/inner/pool/job_spec_builder.h>

#include <utility>

#include <cradle/inner/storage/content_helpers.h>

namespace cradle {

cppcoro::task<job_input>
build_job_input(cas_intf& cas, blob serialized_input, std::size_t boundary)
{
    // At or below the boundary travels inline; strictly above is placed in the
    // CAS and conveyed by digest.
    if (serialized_input.size() <= boundary)
    {
        co_return job_input{inline_input{std::move(serialized_input)}};
    }
    digest d = co_await put_content(cas, std::move(serialized_input));
    co_return job_input{digest_input{std::move(d)}};
}

cppcoro::task<std::vector<job_input>>
build_job_inputs(
    cas_intf& cas, std::vector<blob> serialized_inputs, std::size_t boundary)
{
    std::vector<job_input> inputs;
    inputs.reserve(serialized_inputs.size());
    for (auto& serialized_input : serialized_inputs)
    {
        inputs.push_back(co_await build_job_input(
            cas, std::move(serialized_input), boundary));
    }
    co_return inputs;
}

cppcoro::task<job_spec>
build_job_spec(
    cas_intf& cas,
    provider_id provider,
    request_key key,
    context_id context,
    std::vector<blob> serialized_inputs,
    std::size_t boundary)
{
    std::vector<job_input> inputs = co_await build_job_inputs(
        cas, std::move(serialized_inputs), boundary);
    co_return job_spec{
        std::move(provider),
        std::move(key),
        std::move(context),
        std::move(inputs)};
}

} // namespace cradle
