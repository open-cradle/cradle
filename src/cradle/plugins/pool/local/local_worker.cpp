#include <cradle/plugins/pool/local/local_worker.h>

#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include <cereal/types/string.hpp>
#include <fmt/format.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/encodings/cereal.h>
#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/requests/containment_data.h>
#include <cradle/inner/requests/context_base.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/resolve/seri_resolver.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/config_map_from_json.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

namespace {

// Domain name reported by the worker's local context. Only informational for
// local resolution (the worker never resolves remotely), but sync_context_base
// requires a stable reference to a name.
std::string const the_worker_domain_name{"local_pool"};

// A concrete same-machine synchronous context the worker hands to the seri
// resolver. It reuses the pool's inner_resources (so the memory cache and
// provider registry are shared) and never resolves remotely (empty proxy
// name). Provider-agnostic: the worker does not depend on any specific domain.
class pool_worker_context final : public sync_context_base
{
 public:
    explicit pool_worker_context(inner_resources& resources)
        : sync_context_base{resources, nullptr, ""}
    {
    }

    std::string const&
    domain_name() const override
    {
        return the_worker_domain_name;
    }

    service_config
    make_config(bool need_record_lock) const override
    {
        service_config_map config_map{
            {remote_config_keys::DOMAIN_NAME, the_worker_domain_name},
            {remote_config_keys::NEED_RECORD_LOCK, need_record_lock},
        };
        return service_config{config_map};
    }
};
static_assert(ValidFinalContext<pool_worker_context>);

// Writes the job's inputs as the "args" tuple of the serialized request. Each
// input is a serialized value blob; cereal serializes a std::tuple element as
// "tuple_elementN", and a serializable value as its blob, so writing the input
// blobs under those names reproduces exactly what the inline / contained path
// produces for a tuple of resolved values.
struct args_writer
{
    std::vector<blob> const& inputs;

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        for (std::size_t i = 0; i != inputs.size(); ++i)
        {
            archive(cereal::make_nvp(
                "tuple_element" + std::to_string(i), inputs[i]));
        }
    }
};

// Reconstructs the serialized request the provider's resolver expects, from
// the provider uuid and the loaded input blobs. Cereal JSON is name-keyed, so
// writers can include fields that some request shapes ignore: no-retrier /
// non-introspective leaves skip the retrier and title NVPs, while Thinknode-
// style default_retrier + introspective leaves consume them. No containment
// data and no pool name (the worker must not re-pool).
std::string
build_seri_req(provider_id const& provider, std::vector<blob> const& inputs)
{
    std::stringstream os;
    {
        JSONRequestOutputArchive oarchive(os);
        // Resolve through the derived pool plain-args variant uuid so the
        // uniform value-blob args (args_writer) round-trip correctly.
        std::string variant_uuid{pool_variant_uuid_str(provider)};
        oarchive(cereal::make_nvp("uuid", variant_uuid));
        // default_retrier defaults (ignored by no_retrier leaves)
        oarchive(cereal::make_nvp("base_millis", int64_t{100}));
        oarchive(cereal::make_nvp("max_attempts", 9));
        // introspective title (ignored by non-introspective leaves)
        oarchive(cereal::make_nvp("title", std::string{"pooled"}));
        containment_data::save_nothing(oarchive);
        oarchive(cereal::make_nvp("args", args_writer{inputs}));
        oarchive(cereal::make_nvp("has_pool_name", false));
    }
    return os.str();
}

// Summarize job-context config keys for logs without printing secrets.
std::string
describe_job_context_keys(service_config const& config)
{
    std::string result;
    for (auto const& [key, value] : config.get_config_map())
    {
        if (!result.empty())
        {
            result += ", ";
        }
        // Redact known secret keys without depending on Thinknode headers.
        if (key == "thinknode/access_token")
        {
            auto const* token = std::get_if<std::string>(&value);
            result += fmt::format(
                "{}=<redacted,{} chars>",
                key,
                token ? token->size() : 0);
        }
        else if (auto const* s = std::get_if<std::string>(&value))
        {
            result += fmt::format("{}={}", key, *s);
        }
        else if (auto const* n = std::get_if<std::size_t>(&value))
        {
            result += fmt::format("{}={}", key, *n);
        }
        else if (auto const* b = std::get_if<bool>(&value))
        {
            result += fmt::format("{}={}", key, *b);
        }
        else
        {
            result += key;
        }
    }
    return result;
}

// Prefer a domain-built local sync context when the job carries a serialized
// remote config (e.g. Thinknode session). Fall back to the provider-agnostic
// pool_worker_context when the context field is empty or not usable.
std::shared_ptr<local_context_intf>
make_worker_context(inner_resources& resources, context_id const& context)
{
    auto logger{ensure_logger("local_pool")};
    if (context.empty())
    {
        logger->warn(
            "pool worker: job context empty; using generic pool_worker_context");
        return std::make_shared<pool_worker_context>(resources);
    }
    try
    {
        service_config config{read_config_map_from_json(context)};
        auto domain_name{
            config.get_mandatory_string(remote_config_keys::DOMAIN_NAME)};
        logger->warn(
            "pool worker: rebuilding domain context '{}' ({})",
            domain_name,
            describe_job_context_keys(config));
        auto& dom = resources.find_domain(domain_name);
        auto ctx{dom.make_local_sync_context(config)};
        logger->warn(
            "pool worker: domain context ready; ctx.domain_name()='{}'",
            ctx->domain_name());
        return ctx;
    }
    catch (std::exception const& e)
    {
        // Keep the fallback for now, but make the swallowed failure visible:
        // a generic pool_worker_context will typically fail later with a
        // Thinknode cast error if the provider needs a domain context.
        logger->error(
            "pool worker: failed to rebuild domain context from job "
            "context ({} chars): [{}] {}; falling back to "
            "pool_worker_context",
            context.size(),
            typeid(e).name(),
            e.what());
        return std::make_shared<pool_worker_context>(resources);
    }
}

// Loads one job input: inline bytes travel directly; a digest names content
// that must already be present in the CAS (a missing digest is a genuine
// fault).
cppcoro::task<blob>
load_input(cas_intf& cas, job_input const& input)
{
    if (auto const* inl = std::get_if<inline_input>(&input))
    {
        co_return inl->bytes;
    }
    auto const& dig = std::get<digest_input>(input);
    auto opt_content = co_await cas.get(dig.content_digest);
    if (!opt_content)
    {
        throw std::runtime_error{
            "pool job input content missing from content-addressable store"};
    }
    co_return *opt_content;
}

// Reads the job's current record and reports whether it is already in a
// terminal state. A missing record is not terminal.
cppcoro::task<bool>
current_is_terminal(mutable_store_intf& store, job_id const& id)
{
    auto opt_value = co_await store.get(job_record_key(id));
    if (!opt_value)
    {
        co_return false;
    }
    job_record current{deserialize_job_record(*opt_value)};
    co_return is_terminal(current.status);
}

// Writes the job's record with the given status (and optional output digest or
// error), but never overwrites a record that is already terminal: this honors
// the invariant that a job's status is not observed to leave a terminal state
// (e.g. a cancel that already wrote a terminal record must not be clobbered).
// May throw only on a genuine backend fault in the mutable store.
cppcoro::task<void>
record_status(
    mutable_store_intf& store,
    job_id const& id,
    request_key const& key,
    std::string const& pool_name,
    job_status status,
    digest output_digest,
    std::string error)
{
    // Best-effort guard: a residual read-then-write (TOCTOU) window remains
    // because the mutable store has no atomic compare-and-set.
    if (co_await current_is_terminal(store, id))
    {
        co_return;
    }
    job_record record;
    record.id = id;
    record.key = key;
    record.pool_name = pool_name;
    record.status = status;
    record.output_digest = std::move(output_digest);
    record.error = std::move(error);
    co_await store.put(job_record_key(id), serialize_job_record(record));
}

} // namespace

local_worker::local_worker(
    inner_resources& resources,
    cas_intf& cas,
    ac_intf& ac,
    mutable_store_intf& mutable_store,
    std::string pool_name)
    : resources_{resources},
      cas_{cas},
      ac_{ac},
      mutable_store_{mutable_store},
      pool_name_{std::move(pool_name)}
{
}

cppcoro::task<void>
local_worker::execute(job_spec spec, job_id id)
{
    auto logger{ensure_logger("local_pool")};

    // If cancellation already wrote a terminal record before dispatch, abandon
    // execution: do not run the provider and do not write any status.
    if (co_await current_is_terminal(mutable_store_, id))
    {
        logger->warn(
            "pool worker: job {} already terminal; skipping execute", id);
        co_return;
    }

    logger->warn(
        "pool worker: execute job {} pool='{}' provider='{}' "
        "variant='{}' key='{}' context_chars={} inputs={}",
        id,
        pool_name_,
        spec.provider,
        pool_variant_uuid_str(spec.provider),
        spec.key,
        spec.context.size(),
        spec.inputs.size());

    co_await record_status(
        mutable_store_,
        id,
        spec.key,
        pool_name_,
        job_status::running,
        digest{},
        std::string{});

    blob result_value;
    std::optional<std::string> failure_error;
    try
    {
        std::vector<blob> inputs;
        inputs.reserve(spec.inputs.size());
        for (std::size_t i = 0; i != spec.inputs.size(); ++i)
        {
            auto const& input = spec.inputs[i];
            bool const inline_conveyed
                = std::holds_alternative<inline_input>(input);
            inputs.push_back(co_await load_input(cas_, input));
            logger->warn(
                "pool worker: job {} input[{}] {} size={}",
                id,
                i,
                inline_conveyed ? "inline" : "digest",
                inputs.back().size());
        }
        std::string seri_req{build_seri_req(spec.provider, inputs)};
        logger->warn(
            "pool worker: job {} rebuilt seri_req chars={}",
            id,
            seri_req.size());
        auto ctx{make_worker_context(resources_, spec.context)};
        auto const variant_uuid{pool_variant_uuid_str(spec.provider)};
        auto resolver{
            resources_.get_seri_registry()->find_resolver(variant_uuid)};
        logger->warn(
            "pool worker: job {} resolving via '{}' on domain '{}'",
            id,
            variant_uuid,
            ctx->domain_name());
        auto seri_result{co_await resolver->resolve(
            *ctx, std::move(seri_req), seri_cache_record_lock_t{})};
        result_value = seri_result.value();
        logger->warn(
            "pool worker: job {} provider returned {} bytes",
            id,
            result_value.size());
    }
    catch (std::exception const& e)
    {
        // A provider failure is recorded as a failed job record; no result
        // digest and no AC association are written, so the job is never
        // observable as succeeded. The record is written after the handler
        // because co_await is not permitted inside a catch handler.
        logger->error(
            "pool worker: job {} provider failed: [{}] {}",
            id,
            typeid(e).name(),
            e.what());
        failure_error = e.what();
    }

    if (failure_error)
    {
        co_await record_status(
            mutable_store_,
            id,
            spec.key,
            pool_name_,
            job_status::failed,
            digest{},
            std::move(*failure_error));
        co_return;
    }

    // Success: store the result in the CAS, record the request-key to
    // result-digest association in the AC, then set the terminal status.
    digest output_digest{co_await put_content(cas_, result_value)};
    co_await ac_.put(spec.key, output_digest);
    co_await record_status(
        mutable_store_,
        id,
        spec.key,
        pool_name_,
        job_status::succeeded,
        output_digest,
        std::string{});
    logger->warn("pool worker: job {} succeeded", id);
}

std::unique_ptr<local_worker>
make_local_worker(
    inner_resources& resources,
    cas_intf& cas,
    ac_intf& ac,
    mutable_store_intf& mutable_store,
    std::string pool_name)
{
    return std::make_unique<local_worker>(
        resources, cas, ac, mutable_store, std::move(pool_name));
}

} // namespace cradle
