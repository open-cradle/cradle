#include <msgpack.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_packer.h>
#include <cradle/inner/pool/job_record.h>

namespace cradle {

bool
is_terminal(job_status status)
{
    switch (status)
    {
        case job_status::succeeded:
        case job_status::failed:
        case job_status::cancelled:
            return true;
        case job_status::queued:
        case job_status::running:
            return false;
    }
    return false;
}

std::string
job_record_key(job_id const& id)
{
    return "jobs/" + id;
}

mutable_value
serialize_job_record(job_record const& record)
{
    // The record is packed as a fixed-length array of its fields, in a stable
    // order, so that deserialize_job_record can validate and read them back.
    msgpack_ostream os;
    msgpack_packer packer{os, false};
    packer.pack_array(6);
    packer.pack(record.id);
    packer.pack(record.key);
    packer.pack(record.pool_name);
    packer.pack(static_cast<std::uint8_t>(record.status));
    packer.pack(record.output_digest);
    packer.pack(record.error);
    return std::move(os).get_blob();
}

job_record
deserialize_job_record(mutable_value const& value)
{
    // msgpack::unpack throws on malformed bytes; the shape/range checks below
    // throw on well-formed but invalid content.
    msgpack::object_handle oh = msgpack::unpack(
        reinterpret_cast<char const*>(value.data()), value.size());
    msgpack::object const obj = oh.get();
    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 6)
    {
        throw msgpack::type_error();
    }
    msgpack::object const* const fields = obj.via.array.ptr;
    job_record record;
    fields[0].convert(record.id);
    fields[1].convert(record.key);
    fields[2].convert(record.pool_name);
    std::uint8_t status_value{};
    fields[3].convert(status_value);
    if (status_value > static_cast<std::uint8_t>(job_status::cancelled))
    {
        throw msgpack::type_error();
    }
    record.status = static_cast<job_status>(status_value);
    fields[4].convert(record.output_digest);
    fields[5].convert(record.error);
    return record;
}

} // namespace cradle
