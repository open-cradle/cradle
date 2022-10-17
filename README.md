# CRADLE

[![MSVC](https://github.com/mghro/cradle/actions/workflows/msvc.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/msvc.yml)
[![GCC](https://github.com/mghro/cradle/actions/workflows/gcc.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/gcc.yml)
[![Clang](https://github.com/mghro/cradle/actions/workflows/clang.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/clang.yml)
[![Docker](https://github.com/mghro/cradle/actions/workflows/docker.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/docker.yml)
[![Python](https://github.com/mghro/cradle/actions/workflows/python.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/python.yml)
[![Code Coverage](https://codecov.io/gh/mghro/cradle/branch/main/graph/badge.svg)](https://codecov.io/gh/mghro/cradle)

## Abstract

CRADLE is a framework that intends to speed up lengthy calculations.
A calculation is modeled as a tree of subcalculations. Results of these subcalculations
are cached, so a following calculation that is similar to the previous one can use
these cached subresults, instead of calculating them again.

CRADLE is being used in the context of cancer treatment plans, where it achieves to bring
down the duration of re-calculations from several hours to several minutes.
In this context, a subcalculation is represented by a Thinknode request, so CRADLE
acts as a caching proxy for Thinknode.

More recent developments try to generalize CRADLE, so that it hopefully also becomes useful in other domains.
A calculation is now modeled as a [Merkle tree](https://en.wikipedia.org/wiki/Merkle_tree)
of _requests_, where resolving a request means calling a C++ _function_, which yields a _value_.

- A leaf in the Merkle tree is a constant value. This could be as simple as an integer constant.
  Another possibility is an image that is stored in unmodifiable form somewhere on the internet,
  and identified by its URL.
- A branch in the Merkle tree corresponds to a C++ function that takes one or more subrequests
  as its arguments, and returns a value.

CRADLE offers two caches: a memory cache and a disk cache.

The memory cache stores results in their C++ native format (embedded in an `std::any`),
contained in an `std::unordered_map`. Advantages are:

- No need to serialize the values.
- The hash need not be unique, a "cheaper" implementation suffices.

A nice property of the memory cache is that when the same subrequest appears
multiple times in the calculation tree, and it is not yet cached, then it will be evaluated
only once; all other threads will block until the result is available.

The disk cache is a key-value cache, implemented using SQLite. A key is represented by a hash
that must remain valid between application runs. Values have to be serialized (e.g. using
[cereal](https://uscilab.github.io/cereal/)).

A request has an associated caching level, which is one of the following:

- Uncached. Appropriate for leaves in the calculation tree; could also be used for fast calculations.
- Memory cache only.
- Fully cached (memory cache and disk cache).

Caching on disk, but not in memory, is not possible with the CRADLE design.

There is a similarity between CRADLE and build systems like [Bazel](https://bazel.build/)
or [clearmake](https://help.hcltechsw.com/versionvault/2.0.1/oxy_ex-1/com.ibm.rational.clearcase.tutorial.doc/topics/a_clearmake.html).
These build systems also model their work as a Merkle tree, and cache intermediate results.
Nodes in the tree correspond to a file:

- A leaf in the tree corresponds to a source file.
- Evaluating a branch in the tree means invoking a compiler or other tool that takes one
  or more files as input, and produces another file.


## Build Requirements

### Supported OSs and C++ Compilers

The following platforms/compilers are supported for building CRADLE:

- Ubuntu 20.04 (Focal), using GCC 10 or Clang 14, or higher
- Windows 10, using VS 2019 or higher

Other platforms (e.g., other Linux distros) will likely work with some
adaptation.

CRADLE makes use of some C++20 features, so other compilers (or earlier
versions of the supported compilers) likely won't work.

### CMake

CRADLE is built using CMake and requires a relatively recent version. It's
known to work on 3.18 and 3.19. It should work on more recent versions as well.

### OCaml

CRADLE has a custom preprocessor that's written in OCaml, so the build process
currently require OCaml to be installed. It can be installed available via a
package manager.

#### Ubuntu

```shell
sudo apt-get install -y ocaml-nox
```

#### Windows (via Chocolatey)

```shell
choco install ocpwin
```

### Python

CRADLE uses Python (3.x) both indirectly (via Conan) and directly (for some
testing purposes).

#### Ubuntu

Python itself should be available by default. If necessary, install the
`virtualenv` tool as follows:

```shell
sudo pip3 install virtualenv
```

You can then use the provided CRADLE script to set up a virtual environment and
`source` it:

```shell
scripts/set-up-python.sh --python=python3
source .venv/bin/activate
```

#### Windows

Install Python 3.x however you prefer, e.g.:

```shell
choco install python3
```

No script is currently provided to set up a virtual environment on Windows, but
you can do so yourself if you like.

Either way, you should install the following packages:

```shell
pip install conan gcovr pytest websocket-client msgpack
```

## Secrets

A couple of Thinknode secrets are currently required to build and test CRADLE.
Be sure to acquire them. You have two options for passing them into the build
system:

You can store them in environment variables:

- `CRADLE_THINKNODE_TOKEN`
- `CRADLE_THINKNODE_DOCKER_AUTH`

Or, alternatively, you can store them in files within the root `cradle`
directory:

- `.token`
- `.docker-auth`

(Each file should contain just the secret string.)

## Building

Once you have everything set up, actually building follows the standard CMake
form:

```shell
cmake -Bbuild .
```

or

```shell
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release .
```

You should also be able to use the CMake integration tools in Visual Studio
Code (tested) or Visual Studio (untested).

## Testing

You can build/run the unit tests using the `unit_tests` CMake target, either by
selecting it in your IDE or invoking it with CMake:

```shell
cmake --build build --config Release --target unit_tests
```

(There is also an `integration_tests` target, but that is a bit outdated at the
moment, and it's not really clear that the current form has any purpose in the
project moving forward.)
