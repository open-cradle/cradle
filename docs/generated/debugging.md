# Debugging tips

## Set logging level
To get more output from a test runner or a server, increase the logging level:
```bash
$ export SPDLOG_LEVEL=debug
```
Other levels include `info` and `warn` (the default).

## Run the RPC server in a separate terminal
Some unit tests start an RPC server in testing mode if it is not yet running, and interact with it.
To see what the RPC server is doing, start it in a separate terminal with an appropriate debugging level:
```bash
$ cd some-build-directory
$ export SPDLOG_LEVEL=debug
$ ./rpclib_server --testing
```
then run the test runner in another terminal.

A test runner will abort if it finds a running RPC server that it considers incompatible,
reporting a "code version mismatch". If so, kill the server process.
