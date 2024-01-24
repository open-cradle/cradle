# Remote cache
CRADLE can store results in a remote cache that it accesses via HTTP.
This has been tested on [bazel-remote](https://github.com/buchgr/bazel-remote)
but other HTTP servers might also work.
bazel-remote caches data on a local disk and optionally on a cloud backend: Amazon S3, Google Cloud or Microsoft Azure.

## CRADLE configuration
The mechanism is selected by setting CRADLE's `secondary_cache/factory` configuration option to `http_cache`.
In a [configuration file](config_template.toml), this would be

```
[secondary_cache]
factory = "http_cache"
```

For `rpclib_server`, remote caching is enabled by passing `--secondary-cache http_cache`.

The HTTP port (default: 9090) can be set via the `http_cache/port` option.

## bazel-remote
bazel-remote uses a directory structure on a local disk as secondary storage, and optionally
a cloud-based backend as tertiary storage. Thus, it will not access the cloud when an item
already exists on the local disk.

bazel-remote was written as a remote cache for the [Bazel](https://bazel.build/) build system.
Bazel has many similarities to CRADLE. A Bazel _action_ is the equivalent of a CRADLE request.
Bazel performs an action by invoking a command like a compiler.
The output of an action is formed by its result metadata, and one or more output files.
The CRADLE equivalent is resolving a request, which gives a single output, and no metadata.

When Bazel caches action results, it does so in two steps. First, it stores the metadata,
indexed by a SHA256 hash over the action. The metadata refers to the output files using SHA256
hashes over the file contents; these hashes are used as indices for storing the file data.
Metadata is stored under `/ac/` (AC: action cache) and output files are stored under `/cas/`
(CAS: content-addressable storage).

The CRADLE story is a bit simpler: the output of an action is just a single value. Instead of
converting this value to and from a Bazel action result, CRADLE stores just the value's SHA256 digest
in the AC. bazel-remote default behavior is to reject AC entries that do not look like valid
metadata. Fortunately, this validation can be disabled, by passing `--disable_http_ac_validation`.

### Caching on a local disk
bazel-remote is available as a Docker container. 

The following command starts the container, instructing it to use a local directory only:
```
docker run -u 1000:1000 -v /path/to/cache/dir:/data -p 9090:8080 buchgr/bazel-remote-cache \
--max_size 1 --disable_http_ac_validation
```

bazel-remote has many other configuration options; see [the documentation](https://github.com/buchgr/bazel-remote)
for more information.

CRADLE results are stored under the `/path/to/cache/dir` directory. There are three
subdirectories:

* `cas.v2`: CAS
* `ac.v2`: validated action result metadata
* `raw.v2`: unvalidated action result metadata

As explained above, CRADLE results will end up in `raw.v2` and `cas.v2`; `ac.v2`
will not contain any files.

### Caching on S3
The following command starts the bazel-remote container, instructing it to use a local directory
for secondary storage, and an Amazon S3 bucket for tertiary storage:

```
docker run -u 1000:1000 \
-v /path/to/cache/dir:/data \
-v $HOME/.aws:/aws-config \
-p 9090:8080 buchgr/bazel-remote-cache \
--max_size 1 --disable_http_ac_validation \
--s3.auth_method=aws_credentials_file \
--s3.aws_shared_credentials_file=/aws-config/credentials \
--s3.endpoint=s3.eu-central-1.amazonaws.com \
--s3.bucket=my-bucket
```

The local `/path/to/cache/dir` directory will be as in the previous use-case.
Credentials should be in `~/.aws/credentials` as

```
[default]
aws_access_key_id=...
aws_secret_access_key=...
```
