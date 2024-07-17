The [msgpack-cxx](https://github.com/msgpack/msgpack-c/wiki) library
has two mechanisms to support new class types: a non-intrusive and an intrusive one.
The library selects the non-intrusive approach if available, otherwise the intrusive one.

## Non-intrusive

The non-intrusive approach is to add two or three function specializations inside a nested `namespace adaptor`:

```
struct convert<T>::operator()(msgpack::object const& o, T& v) const;

struct pack<T>::operator()(msgpack::packer<Stream>& o, T const& v) const;

struct object_with_zone<T>::operator()(msgpack::object::with_zone& o, T const& v) const;
```

The last one may not be needed. See `msgpack_adaptors_main.h` for the specialization for `T` = `cradle::blob`.


## Intrusive

The intrusive approach doesn't seem to be documented, but it means adding two member functions to the class
that is to be serialized:

```
template<typename Stream>
void
msgpack_pack(msgpack::packer<Stream>& packer) const;

void
msgpack_unpack(msgpack::object const& msgpack_obj);
```

In CRADLE, `Stream` will always be `msgpack_ostream`, which means that `msgpack_pack()` can be
a non-template function. See `class function_request` for an example.

The documentation does describe a helper macro `MSGPACK_DEFINE`, which implements the intrusive
approach for trivial cases. See `demo_class.h` for an example.
