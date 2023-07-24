# Example: make some blob
This section describes an example function, and what should be done to integrate it with
CRADLE. The function is called `make_some_blob`; it simply allocates a memory region and
fills it with a data pattern. The function is embedded in a request that can be resolved
locally (in-process) or remotely (across an RPC channel).

Steps:
* Create the function
* Create a request class embedding the function
* Register the class so that it can be deserialized
* Link the new code with a test runner and the RPC server
* Create a unit test
* Run the unit test

Variants:
* Asynchronous resolution
* Cancellable request
* Subrequests

This section uses `${CRADLE_REPO}` to denote the CRADLE repository's root directory, containing
e.g. the main `CMakeLists.txt`, and the `src` and `tests` directories.

## The function
It all starts with a C++ function implementing the desired functionality. This function
is intended to be used in unit tests, so is defined in `plugins/domain/testing/requests.cpp`:

```C++
cppcoro::task<blob>
make_some_blob(context_intf& ctx, std::size_t size, bool use_shared_memory)
{
    spdlog::get("cradle")->info(
        "make_some_blob({}, {})", size, use_shared_memory);
    auto& loc_ctx{cast_ctx_to_ref<local_context_intf>(ctx)};
    auto owner = loc_ctx.make_data_owner(size, use_shared_memory);
    auto* data = owner->data();
    uint8_t v = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        data[i] = v;
        v = v * 3 + 1;
    }
    co_return blob{std::move(owner), as_bytes(data), size};
}
```

Let's look at the various code parts. The first one is the function header:
```C++
cppcoro::task<blob>
make_some_blob(context_intf& ctx, std::size_t size, bool use_shared_memory)
```
The function is a coroutine yielding a blob (a memory region containing
unstructured data). It has three arguments:
* `ctx` is the [context](context.md), providing support that the function needs;
  in this case, a shared memory allocator.
* The required blob `size`.
* `use_shared_memory` indicates whether the blob should be allocated in shared
  memory (useful for large blobs referenced by different processes) or in local
  memory only.

Next comes
```C++
spdlog::get("cradle")->info(
    "make_some_blob({}, {})", size, use_shared_memory);
```
CRADLE uses the [spdlog](https://github.com/gabime/spdlog) logging library.
When the logging level is high enough (e.g. `SPDLOG_LEVEL` is set to `info`),
the output appears in the terminal.

```C++
auto& loc_ctx{cast_ctx_to_ref<local_context_intf>(ctx)};
```
The caller has to ensure that the context class implements all needed
interfaces. Trying to locally resolve a request with a context class that
does _not_ implement `local_context_intf` doesn't make sense, so there
is no good reason why the cast should fail.

```C++
auto owner = loc_ctx.make_data_owner(size, use_shared_memory);
auto* data = owner->data();
```
This allocates the memory region, and obtains an owner for the region,
plus a pointer to the allocated data. The memory region's lifetime coincides
with that of its owner.

```C++
uint8_t v = 0;
for (std::size_t i = 0; i < size; ++i)
{
    data[i] = v;
    v = v * 3 + 1;
}
```
The memory region is filled with some data. Finally,

```C++
co_return blob{std::move(owner), as_bytes(data), size};
```
returns a `blob` object referring to the memory region. Blobs are immutable: they do
not offer functionality to modify the underlying data.


## The request
The next step is to embed the function in a request class, which is done in
`plugins/domain/testing/requests.h`:

```C++
template<caching_level_type Level>
auto
rq_make_some_blob(std::size_t size, bool shared)
{
    using props_type = request_props<Level, true, true>;
    request_uuid uuid{"make_some_blob"};
    uuid.set_level(Level);
    std::string title{"make_some_blob"};
    return rq_function_erased(
        props_type(std::move(uuid), std::move(title)),
        make_some_blob,
        size,
        shared);
}
```

This defines a factory function creating an object of a `function_request_erased`
template class instantiation.

```C++
template<caching_level_type Level>
auto
rq_make_some_blob(std::size_t size, bool use_shared_memory)
```
The factory function is a template function having the caching level as template
parameter. Real-life requests probably will always be fully cached, so won't
need this parameter, but for testing purposes it's useful.
The two arguments are passed to `make_some_blob()`.


```C++
using props_type = request_props<Level, true, true>;
```
`props_type` defines several request properties:
* The caching level.
* A boolean indicating whether the function is a coroutine or a normal C++ function.
  In this case it's a coroutine, so the value is set to true.
* A boolean indicating whether the request needs introspection support.
  In this case it does.

```C++
request_uuid uuid{"make_some_blob"};
uuid.set_level(Level);
```
The request class needs a unique identification. Here it's based on `make_some_blob`
which is good enough for testing purposes, but for a real-life request it should e.g.
be an RFC 4122 version 4 (random) value. The exact class depends on the caching level;
the UUID should do so too, so the `request_uuid` value is modified with the level.

```C++
std::string title{"make_some_blob"};
```
Defines the title under which this request is named in introspection output.

```C++
return rq_function_erased(
    props_type(std::move(uuid), std::move(title)),
    make_some_blob,
    size,
    use_shared_memory);
```
Creates the appropriate `function_request_erased` instantiation. The object embeds
the C++ function (`make_some_blob`) and the function's arguments (`size` and `use_shared_memory`).

## Registering the request classes
It should be possible to remotely resolve the new request. This implies that the request
classes have to be registered, so that the deserialization process is able, given
a UUID value, to create request objects of the correct types. The registration happens in
`plugins/domain/testing/seri_catalog.cpp`:

```C++
void
register_testing_seri_resolvers()
{
    register_seri_resolver(
        rq_make_some_blob<caching_level_type::none>(1, false));
    register_seri_resolver(
        rq_make_some_blob<caching_level_type::memory>(1, false));
    register_seri_resolver(
        rq_make_some_blob<caching_level_type::full>(1, false));
}
```
As the request supports all three caching levels, and the request types depend on the
level, the request has to be registered three times. The values passed for `size`
and `use_shared_memory` are irrelevant here.

The application where the deserialization occurs (in this case, the test runner) has
to call `register_testing_seri_resolvers()` in its initialization.

## Linking the new code
CRADLE currently uses static linking only. A unit test for the `make_some_blob` functionality
must link the new code into the corresponding test runner.

All of this new code resides in `plugins/domain/testing/*.cpp`. The main `CMakeLists.txt` puts this
in the `cradle_plugins_inner` library:
```CMake
file(GLOB_RECURSE srcs_plugins_inner CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/domain/testing/*.cpp")
add_library(cradle_plugins_inner STATIC ${srcs_plugins_inner})
```
which the test runners link against. In `tests/CMakeLists.txt`:
```CMake
target_link_libraries(test_lib_inner PUBLIC
    cradle_plugins_inner
    ...)

target_link_libraries(inner_test_runner PRIVATE
    ...
    test_lib_inner)
```

For the new requests to be resolved on an RPC server, the code must also be present
in the `rpclib_server` executable; the main `CMakeLists.txt` therefore contains
```CMake
target_link_libraries(rpclib_server
    ...
    cradle_plugins_inner
    ...)
```

## Testing and resolving the new request
`tests/rpclib/proxy.cpp` contains unit tests for resolving a "make some blob" request
on an RPC server:
```C++
static void
test_make_some_blob(bool use_shared_memory)
{
    constexpr auto caching_level{caching_level_type::full};
    constexpr auto remotely{true};
    std::string proxy_name{"rpclib"};
    register_testing_seri_resolvers();
    inner_resources service;
    init_test_inner_service(service);
    register_rpclib_client(make_inner_tests_config(), service);
    testing_request_context ctx{service, nullptr, remotely, proxy_name};

    auto req{rq_make_some_blob<caching_level>(10000, use_shared_memory)};
    auto response = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(response.size() == 10000);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
    REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
}

TEST_CASE("resolve to a plain blob", "[rpclib]")
{
    test_make_some_blob(false);
}

TEST_CASE("resolve to a blob file", "[rpclib]")
{
    test_make_some_blob(true);
}
```
CRADLE uses [Catch2](https://github.com/catchorg/Catch2) for its unit tests.
There are two test cases; the first one resolves a "make some blob" request on an
RPC server and retrieves the result over the RPC channel. The second unit test
is similar, but the resulting blob is now transferred via shared memory.

Taking apart the common code in `test_make_some_blob()`:
```C++
constexpr auto caching_level{caching_level_type::full};
```
The caching level used for resolving the request on the server. For a request like this one,
which has the caching level as template parameter, the used level has to be one with
which the request is registered in `register_testing_seri_resolvers()`.

```C++
constexpr auto remotely{true};
std::string proxy_name{"rpclib"};
```
The request should be resolved remotely, on the server identified as "rpclib".

```C++
register_testing_seri_resolvers();
```
Registers the request for deserialization, as explained above.

```C++
inner_resources service;
init_test_inner_service(service);
```
Creates and initializes the resources available for resolving the request.

```C++
register_rpclib_client(make_inner_tests_config(), service);
```
Makes the rpclib client (proxy) available in `service`.

```C++
testing_request_context ctx{service, nullptr, remotely, proxy_name};
```
Creates the context providing the resources for resolving the request.

```C++
auto req{rq_make_some_blob<caching_level>(10000, use_shared_memory)};
```
The request itself. A 10000-bytes blob should be created, in local or shared memory,
depending on `use_shared_memory`.

```C++
auto response = cppcoro::sync_wait(resolve_request(ctx, req));
```
Resolves the request. `resolve_request()` is the main API for this; there should
be no reason for user code to call any lower-level function. `resolve_request` is
a coroutine, so `cppcoro::sync_wait` is needed to make the response available in
a non-coroutine context.

```C++
REQUIRE(response.size() == 10000);
REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
```
The response should be a blob with the requested size, and correct contents.

## Running the unit tests
After building the unit test runner and the RPC server, the tests can be run.
Assuming that the build directory is `build-debug`:
```bash
$ cd ${CRADLE_REPO}
$ cmake --build build-debug --target inner_test_runner --target rpclib_server
$ cd build-debug
$ ./tests/inner_test_runner '[rpclib]'
...
All tests passed (11 assertions in 7 test cases)
```
This also runs the other five unit tests in `tests/rpclib/proxy.cpp`, hence seven test cases in total.

Alternatively, the new unit tests can be run with all other "inner" tests, through
```bash
$ cmake --build build-debug --target inner_tests
...
All tests passed (1855 assertions in 194 test cases)

[100%] Built target inner_tests
```

The test runner starts and stops `rpclib_server` in each test case that needs it.
It is also possible to start the server manually; the test runner then uses the already running server instance:
```bash
$ cd ${CRADLE_REPO}/build-debug
$ ./rpclib_server --testing
# Switch to another terminal
$ cd ${CRADLE_REPO}/build-debug
$ ./tests/inner_test_runner '[rpclib]'
```
This is especially useful for looking at the logging generated by the server, which otherwise is not available.

## Asynchronous request resolution
The example so far performs a synchronous request resolution: there is a blocking `resolve_request()` call,
taking a context argument that is associated with all subrequests in the tree.

An alternative is an asynchronous request resolution. Each subrequest now has a corresponding context
object, and progress of the subrequest's resolution can be queried through that context. In addition,
there still is a "tree-level context" applying to the entire context tree.

For the test example, this means to replace
```C++
testing_request_context ctx{service, nullptr, remotely, proxy_name};
```
with 
```C++
auto tree_ctx{std::make_shared<proxy_atst_tree_context>(service, proxy_name)};
auto ctx{root_proxy_atst_context{tree_ctx}};
```
There still is a blocking `resolve_request()` call, but now another thread can
query the status of the context tree:
* `ctx.get_status_coro()` retrieves the status of a subrequest resolution;
* `ctx.get_num_subs()` and `ctx.get_sub(i)` obtain references to subcontexts,
  ready for querying status or retrieving sub-subcontexts.

Example code can be found in `tests/inner/resolve/resolve_async.cpp`.

## Cancelling a request resolution
Another advantage of asynchronously resolving a request is that the operation can be cancelled.
This requires cooperation from the request's function, though: it must check on polling
basis whether cancellation is requested, and act on it if so.

This means that the function will typically look like
```C++
cppcoro::task<value_type>
request_function(context_intf& ctx, ...)
{
    auto& cctx{cast_ctx_to_ref<local_async_context_intf>(ctx)};
    ...
    for ( ; ; )
    {
        // ... do some work ...
        if (cctx.is_cancellation_requested())
        {
            // ... prepare cancellation ...
            cctx.throw_async_cancelled();
        }
    }
    ...
    co_return result;
}
```

An example is `cancellable_coro()` in `plugins/domain/testing/requests.cpp`.

To cancel a request resolution, a thread should call one of
* `local_async_context_intf::request_cancellation()` (local resolution only)
* `co_await async_context_intf::request_cancellation_coro()` (local or remote resolution)

The cancellation request should be picked up by the request function, and result
in an `async_cancelled` exception being thrown from the `resolve_request()` call.

Example code can be found in `tests/inner/resolve/resolve_async.cpp`.

## Subrequests
The `size` argument to `rq_make_some_blob` until now was a simple `std::size_t`
value, but it could also be a subrequest. To support that, first replace
```C++
template<caching_level_type Level>
auto
rq_make_some_blob(std::size_t size, bool use_shared_memory)
```
in `plugins/domain/testing/requests.h` with
```C++
template<caching_level_type Level, typename Size>
    requires TypedArg<Size, std::size_t>
auto
rq_make_some_blob(Size size, bool use_shared_memory)
```
The `requires` indicates that `Size` should either be a literal `std::size_t` value, or
a subrequest yielding such a value.

To pass a subrequest from the test, replace
```C++
auto req{rq_make_some_blob<caching_level>(10000, use_shared_memory)};
```
in `tests/rpclib/proxy.cpp` with
```C++
std::size_t const size{10000};
auto req{rq_make_some_blob<caching_level>(rq_value(size), use_shared_memory)};
```
thus replacing the literal value with a simple `rq_value`-based subrequest.

This builds, but when running gives a `conflicting types for uuid make_some_blob+full`
error. The reason is that replacing the literal 10000 with a subrequest changes
the type of the `function_request_erased` instantiation that is created by
`rq_make_some_blob`. There are now two types for the one
`make_some_blob+full` UUID, and that is illegal.

The solution is to normalize the `size` argument so that it always becomes a
`function_request_erased` subrequest. In `rq_make_some_blob()`, replace the final statement
```C++
return rq_function_erased(
    props_type(std::move(uuid), std::move(title)),
    make_some_blob,
    size,
    use_shared_memory);
```
with
```C++
return rq_function_erased(
    props_type(std::move(uuid), std::move(title)),
    make_some_blob,
    normalize_arg<std::size_t, props_type>(std::move(size)),
    use_shared_memory);
```
Now the build fails. The exact error message depends on the compiler, but somewhere at
the beginning there should be something like
```
‘coro’ is not a member of ‘cradle::normalization_uuid_str<long unsigned int>’
```
This indicates that there is no `normalization_uuid_str` specialization for `std::size_t`.
The missing specialization should be added to `plugins/domain/testing/normalization_uuid.h`:
```C++
template<>
struct normalization_uuid_str<std::size_t>
{
    static const inline std::string func{"normalization<std::size_t,func>"};
    static const inline std::string coro{"normalization<std::size_t,coro>"};
};
```
With this final change, both the build and the test succeed.
