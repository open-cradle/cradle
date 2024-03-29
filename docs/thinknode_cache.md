# Cache
CRADLE caches objects whenever possible; there are two levels:

* A primary memory cache (default size: 4GB)
* A secondary cache: a local disk cache (default size: 4GB) or a [remote cache](remote_cache.md)

The main local disk-based cache is formed by an SQLite database, which also contains small blobs.
Large blobs are compressed via LZ4 and stored in a file.

For all types of secondary storage, structured data is first serialized into a form similar to the MessagePack
serialization used between client and CRADLE.
(Preferred way forward: no serialization, CRADLE need not understand the data format,
it can store raw data.)

## What does CRADLE cache?
CRADLE caches most Thinknode responses:

Thinknode request                               |  Entry type                       | Link                                                  | Synopsis
-----------------                               | -----------                       | ----                                                  | --------
`GET /iam/contexts/:id`                         | `get_context_contents`            |                                                       | Available apps
`GET /apm/apps/:account/:app/versions/:version` | `get_app_version_info`            |                                                       | App version info
`POST /iss/{:type}`                             | `post_iss_object`                 | [`post_iss_object`](msg_post_iss_object.md)           | Reference id to an immutable object
`GET /iss/:id/immutable`                        | `resolve_iss_object_to_immutable` | [Data](thinknode_data.md)                             | Immutable id
`GET /iss/immutable/:id`                        | `retrieve_immutable`              | [Data](thinknode_data.md)                             | Immutable data as dynamic
`GET /iss/immutable/:id`                        | `retrieve_immutable_blob`         | [Data](thinknode_data.md)                             | Immutable data as blob
`HEAD /iss/:id`                                 | `get_iss_object_metadata`         | [`iss_object_metadata`](msg_iss_object_metadata.md)   | Metadata for an immutable object
`POST /calc/:id`                                | `post_calculation`                | [`post_calculation`](msg_post_calculation.md)         | Calculation request id
`GET /calc/:id`                                 | `retrieve_calculation_request`    | [`calculation_request`](msg_calculation_request.md)   | Calculation descriptor
`GET /calc/:id/status`                          | (not cached)                      |                                                       | Retrieves calculation status
`POST /iss/:id/buckets/:bucket`                 | N/A (see notes)

Notes:

* The _entry type_ identifies what kind of data the cache entry contains. The cache keys are a hash
  calculated over several items, the first one always being the entry type string.
* If the _link_ refers to a CRADLE request, the cache entry contains the data returned in the response to that request.
* "N/A" means that the Thinknode request is issued only for CRADLE requests that are
  candidate for removal according to [the overview](thinknode_msg_overview.md) (see below).

CRADLE also caches:

Entry type                     | Link                                              | Synopsis
----------                     | ----                                              | --------
`local_function_calc`          | [`perform_local_calc`](msg_perform_local_calc.md) | Return value from local function calls
`resolve_named_type_reference` |                                                   | Part of schema returned via `get_app_version_info` (is it useful to store this?)
`coerce_encoded_object`        |                                                   | Conversion of an object passed in a `post_iss_object` request

CRADLE currently also caches information on behalf of messages that are candidate for removal according to [the overview](thinknode_msg_overview.md):

Entry type                          | Status
----------                          | ------
`resolve_results_api_query`         | Astroid
`locally_resolve_results_api_query` | Astroid
`type_contains_references`          | Thinknode
`deeply_copy_iss_object`            | Thinknode
`deeply_copy_calculation`           | Thinknode
