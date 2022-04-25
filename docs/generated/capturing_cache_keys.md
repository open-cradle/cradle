# Capturing cache keys

Cache keys are stored in

* The `shared_task_wrapper` coroutine (`tasklet.h`); if introspection is enabled
* A cache record; if a new record is created on a cache miss
* (Currently/formerly) `struct untyped_immutable_cache_ptr`: looks redundant, should use the key stored in the cache record

Assuming that captures (=clones) are more expensive than passing `shared_ptr`'s around,
we should do the latter, and have `captured_id`'s contain a `shared_ptr`. It's also safer.

For how to pass `capture_id`'s in function calls, we can look at
[R.36](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r36-take-a-const-shared_ptrwidget-parameter-to-express-that-it-might-retain-a-reference-count-to-the-object-)
and similar rules. Conclusions:

* We should pass `captured_id`'s as `captured_id const&` if the called function
  needs to store the id.
* Otherwise, we should pass them as `id_interface const&`.
* Exception: if the called function is a coroutine, we should pass them
  by value, as usual.

  (If we should pass them by const reference, the coroutine would store the
  reference in its coroutine state, and the referenced object might not exist
  anymore when the coroutine is resumed from its initial suspension point.)
