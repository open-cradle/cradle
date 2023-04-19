# Lambda captures

## GCC bug
[Issue #170](https://github.com/open-cradle/cradle/issues/170): crashes occur indicating memory corruption, e.g. "double free".

This looks like a gcc compiler bug, probably [this one](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=98401).

These crashes seem to happen when `co_await`-ing an expression containing a (temporary) lambda taking a (string) argument by value.
The workaround would be to assign the lambda to a variable.

## Capture by value or by reference
When there is a `co_await` on a lambda in the same function that creates the lambda, capturing by reference
should be safe: variables will disappear only after the `co_await` has finished.
Capturing by reference leads to smaller (and probably slightly faster) code.

Otherwise, capturing by value is needed, e.g. when the lambda is passed to `make_shared_task_for_cacheable()`.
