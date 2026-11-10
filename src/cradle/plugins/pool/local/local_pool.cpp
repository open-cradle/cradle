#include <cradle/plugins/pool/local/local_pool.h>

#include <coroutine>
#include <optional>
#include <utility>

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/plugins/pool/local/local_worker.h>

namespace cradle {

namespace {

// Generate a random UUID string to identify an accepted job; unique without
// coordination and uniform across pools and hosts.
job_id
make_random_job_id()
{
    boost::uuids::random_generator gen;
    return boost::lexical_cast<std::string>(gen());
}

} // namespace

local_pool::local_pool(
    std::string name,
    inner_resources& resources,
    cas_intf& cas,
    ac_intf& ac,
    mutable_store_intf& mutable_store,
    pool_settings settings)
    : name_{std::move(name)},
      resources_{resources},
      cas_{cas},
      ac_{ac},
      mutable_store_{mutable_store},
      settings_{std::move(settings)}
{
}

local_pool::~local_pool()
{
    // Wait for all spawned workers to finish so the storage references they
    // hold outlive them.
    cppcoro::sync_wait(async_scope_.join());
}

std::string const&
local_pool::name() const
{
    return name_;
}

cas_intf&
local_pool::cas()
{
    return cas_;
}

ac_intf&
local_pool::ac()
{
    return ac_;
}

mutable_store_intf&
local_pool::mutable_store()
{
    return mutable_store_;
}

cppcoro::task<job_id>
local_pool::submit(job_spec spec)
{
    job_id id{make_random_job_id()};

    job_record record;
    record.id = id;
    record.key = spec.key;
    record.pool_name = name_;
    record.status = job_status::queued;

    co_await mutable_store_.put(
        job_record_key(id), serialize_job_record(record));

    // Dispatch the worker as a fire-and-forget coroutine on the async thread
    // pool; submit returns the id promptly without awaiting the worker.
    async_scope_.spawn(cppcoro::schedule_on(
        resources_.get_async_thread_pool(), run_worker(spec, id)));

    co_return id;
}

cppcoro::task<void>
local_pool::run_worker(job_spec spec, job_id id)
{
    auto worker
        = make_local_worker(resources_, cas_, ac_, mutable_store_, name_);
    co_await worker->execute(std::move(spec), std::move(id));
}

cppcoro::task<void>
local_pool::cancel(job_id id)
{
    std::optional<mutable_value> stored
        = co_await mutable_store_.get(job_record_key(id));
    if (!stored)
    {
        co_return;
    }

    job_record record{deserialize_job_record(*stored)};
    if (is_terminal(record.status))
    {
        co_return;
    }

    record.status = job_status::cancelled;
    co_await mutable_store_.put(
        job_record_key(id), serialize_job_record(record));
}

std::unique_ptr<pool_intf>
make_local_pool(
    std::string name,
    inner_resources& resources,
    cas_intf& cas,
    ac_intf& ac,
    mutable_store_intf& mutable_store,
    pool_settings settings)
{
    return std::make_unique<local_pool>(
        std::move(name),
        resources,
        cas,
        ac,
        mutable_store,
        std::move(settings));
}

} // namespace cradle
