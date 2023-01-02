#ifndef INCLUDE_CRADLE_EXTERNAL_API_H
#define INCLUDE_CRADLE_EXTERNAL_API_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/typing/core/type_definitions.h> // blob
#include <cradle/websocket/types.hpp> // calculation_request

namespace cradle {

namespace external {

// Failures lead to thrown exceptions

/*
 * Documentation for the JSON configuration passed to start_service();
 * possibly outdated and/or incomplete. All values are optional.
 *
 * (int) memory_cache/unused_size_limit
 * The maximum amount of memory to use for caching results that are no
 * longer in use, in bytes.
 *
 * (string) disk_cache/directory
 * (int) disk_cache/size_limit
 * Config for the disk cache plugin
 *
 * (int) request_concurrency
 * How many concurrent threads to use for request handling.
 * The default is one thread for each processor core.
 * TODO remove request_concurrency if really unused
 *
 * (int) compute_concurrency
 * How many concurrent threads to use for computing.
 * The default is one thread for each processor core.
 *
 * (int) http_concurrency
 * How many concurrent threads to use for HTTP requests
 *
 * Example:
 * {
 *     "memory_cache": {
 *         "unused_size_limit": 1073741824
 *     },
 *     "disk_cache": {
 *         "directory": "some_dir",
 *         "size_limit": 1073741824
 *     },
 *     "request_concurrency": 2,
 *     "compute_concurrency": 2,
 *     "http_concurrency": 2
 * }
 */

class api_service_impl;
class api_service
{
    std::unique_ptr<api_service_impl> pimpl_;

 public:
    api_service(std::string json_text);
    api_service(api_service&& that);
    ~api_service();
    api_service_impl&
    impl()
    {
        return *pimpl_;
    }
};

// The service will be stopped when the returned object goes out of scope.
api_service
start_service(std::string json_text);

struct api_thinknode_session_config
{
    std::string api_url;
    std::string access_token;
};

class api_session_impl;
class api_session
{
    std::unique_ptr<api_session_impl> pimpl_;

 public:
    api_session(
        api_service& service, api_thinknode_session_config const& config);
    api_session(api_session&& that);
    ~api_session();
    api_session_impl&
    impl()
    {
        return *pimpl_;
    }
};

// TODO replace this config too with JSON?
api_session
start_session(
    api_service& service, api_thinknode_session_config const& config);

cppcoro::task<std::string>
get_context_id(api_session& session, std::string realm);

cppcoro::shared_task<blob>
get_iss_object(
    api_session& session,
    std::string context_id,
    std::string object_id,
    bool ignore_upgrades = false);

cppcoro::shared_task<std::string>
resolve_iss_object_to_immutable(
    api_session& session,
    std::string context_id,
    std::string object_id,
    bool ignore_upgrades = false);

cppcoro::shared_task<std::map<std::string, std::string>>
get_iss_object_metadata(
    api_session& session, std::string context_id, std::string object_id);

// Returns object_id
cppcoro::shared_task<std::string>
post_iss_object(
    api_session& session,
    std::string context_id,
    std::string schema, // URL-type string
    blob object_data);

cppcoro::task<>
copy_iss_object(
    api_session& session,
    std::string source_context_id,
    std::string destination_context_id,
    std::string object_id);

cppcoro::task<>
copy_calculation(
    api_session& session,
    std::string source_context_id,
    std::string destination_context_id,
    std::string calculation_id);

cppcoro::task<dynamic>
resolve_calc_to_value(
    api_session& session, string context_id, calculation_request request);

cppcoro::task<std::string>
resolve_calc_to_iss_object(
    api_session& session, string context_id, calculation_request request);

cppcoro::task<calculation_request>
retrieve_calculation_request(
    api_session& session, std::string context_id, std::string calculation_id);

} // namespace external

} // namespace cradle

#endif
