#ifndef CRADLE_INNER_REQUESTS_CONTAINTMENT_DATA_H
#define CRADLE_INNER_REQUESTS_CONTAINTMENT_DATA_H

#include <memory>
#include <string>

#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

/*
 * Data needed for calling a request function in contained mode, where
 * the actual function execution occurs in a separate process, being an
 * rpclib_server instance.
 *
 * The function is identified by the uuid for the owning request.
 * plain_uuid is the uuid for a request variant (stored in a seri_registry)
 * taking plain values, no normalized ones. It can thus be different from the
 * request's main uuid.
 * The function should be in a DLL, so the server must be instructed to load
 * that DLL, so needs to know the DLL's location and name.
 */
class containment_data
{
 public:
    containment_data(
        request_uuid plain_uuid, std::string dll_dir, std::string dll_name);

    // Serialize this object
    void
    save(JSONRequestOutputArchive& archive);

    // Serialize the "no containment data" information.
    static void
    save_nothing(JSONRequestOutputArchive& archive);

    // Creates a containment_data object from the serialization for the
    // associated request.
    // Returns an empty unique_ptr if the serialization has no containment
    // data.
    static std::unique_ptr<containment_data>
    load(JSONRequestInputArchive& archive);

    request_uuid const plain_uuid_;
    std::string const dll_dir_;
    std::string const dll_name_;
};

} // namespace cradle

#endif
