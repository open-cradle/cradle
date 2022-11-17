# Shared memory in CRADLE

## Problem description
On the same workstation, we will have several applications using CRADLE, and a single CRADLE server process
offering access to a shared memory cache, a shared disk cache, and possibly additional resources.

We need to share large chunks of data (e.g. describing large images) between applications and the server process.
These chunks could be function arguments or results; in the last case, they should be stored in the central
memory cache. Due to their size, copying these chunks should be avoided.

It would be nice to have an RPC between apps and server that is based on shared memory but no out-of-the-box
solution seems to exist.
A recent (2021) [conference paper](https://sigops.org/s/conferences/hotos/2021/papers/hotos21-s10-wang.pdf)
describes a direction only. It refers to several existing solutions, but these are all part of a
bigger framework and hard to isolate.

## Solution alternatives

### iceoryx
[Eclipse iceoryx™](https://iceoryx.io/v2.0.2/)
is an inter-process-communication middleware that enables virtually limitless data transmissions at constant time.
It's mostly a C++ library, and available from [GitHub](https://github.com/eclipse-iceoryx/iceoryx).

Data is exchanged between publishers and subscribers, or clients and servers.
The C++ API has two flavors: typed and untyped. Both have their disadvantages:

* The typed API has the drawback that "RouDi and all Apps have to be built with the same compiler and the same compiler flags"
  which seems to make it unusable for us. The same drawback would apply to any other approach sharing typed data
  between processes.
* The untyped API does not seem to support passing chunk sizes, which would be a must.

Iceoryx offers a request-response exchange but does not map well to the longer-term chunk storage that we need.

There are other implementations of IPC over shared memory (like Apache Thrift, cpp-ipc, eCAL,
Cyclone DDS, hostppc) but none seem to map well to our needs; in addition, they tend to be
either immature or huge.

### POSIX shared memory
The [POSIX shared memory API](https://man7.org/linux/man-pages/man7/shm_overview.7.html)
is much lower-level but seems to map well to our requirements:

* We would need two kinds of blobs: one for local memory (as we have now), one for shared memory.
* A shared memory blob is identified by an id that maps one-to-one to the name passed to `shm_open()`.
* The id's can be shared between processes.
* The process creating the blob sets it size; other processes can obtain the size via an `fstat()` call.
* Shared memory blobs are immutable, so no synchronization is needed for accessing them.
* The POSIX implementation will keep track of references; once the blob creator has called
  `shm_unlink()` and all references have gone, the shared memory object will be deleted.
  The `shm_unlink()` call must happen only after all interested parties have had their chance
  to create a reference, via an `mmap()` call.

According to the man page, the POSIX shared memory API should be preferred over the older System V one.

### Boost shared memory
[Boost shared memory](https://www.boost.org/doc/libs/1_80_0/doc/html/interprocess/sharedmemorybetweenprocesses.html)
seems to offer the same functionality as the POSIX API, at a somewhat higher abstraction level.
It also supports systems that do not implement the POSIX API.

## Tentative conclusion
I tend to think that the best approach would be a hybrid one: use an existing RPC solution
(not based on shared memory) such as gRPC, and have RPC messages refer to shared memory blob id's.

Typed data should be kept within the process. Data exchanged between processes should
be blobs (by nature or by serialization).
