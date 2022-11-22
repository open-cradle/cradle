# MessagePack

## Issues
- Can msgpack be extended to serialize _any_ value?

## Evaluating version 4.1.2
We are currently using an old version (3.3.0). To upgrade to the latest one available in Conan:
```
CMakeLists.txt
-        msgpack/3.3.0
+        msgpack-cxx/4.1.2
-    msgpack::msgpack
+    msgpackc-cxx::msgpackc-cxx
```

But then the "mixed calcs" test in `tests/unit/websocket/calculations.cpp` fails.

Thinknode returns error code 204, and retrieving more information using the status mechanism gives
```
{
   "failed" : {
      "code" : "422",
      "error" : "invalid_argument",
      "message" : "Validation reported invalid_type error for argument `x` at index 0 during function request argument validation at path \"\" corresponding to position 0 within the stream with the message: Expected `float` value, but got `posfixint` (0x3)"
   }
}
```

The reason can be seen in `include/msgpack/v1/pack.hpp`:
```
54cb4350b (Takatoshi Kondo 2016-01-22 21:44:58 +0900 1161) inline packer<Stream>& packer<Stream>::pack_double(double d)
54cb4350b (Takatoshi Kondo 2016-01-22 21:44:58 +0900 1162) {
d5c837b61 (GeorgFritze     2022-06-03 15:08:23 +0200 1163)     if(d == d) { // check for nan
d5c837b61 (GeorgFritze     2022-06-03 15:08:23 +0200 1164)         // compare d to limits to avoid undefined behaviour
d59e1d771 (GeorgFritze     2022-06-07 09:46:39 +0200 1165)         if(d >= 0 && d <= double(std::numeric_limits<uint64_t>::max()) && d == double(uint64_t(d))) {
68acf21a8 (GeorgFritze     2022-06-02 10:40:51 +0200 1166)             pack_imp_uint64(uint64_t(d));
d5c837b61 (GeorgFritze     2022-06-03 15:08:23 +0200 1167)             return *this;
05de839b4 (GeorgFritze     2022-06-13 09:18:22 +0200 1168)         } else if(d < 0 && d >= double(std::numeric_limits<int64_t>::min()) && d == double(int64_t(d))) {
68acf21a8 (GeorgFritze     2022-06-02 10:40:51 +0200 1169)             pack_imp_int64(int64_t(d));
68acf21a8 (GeorgFritze     2022-06-02 10:40:51 +0200 1170)             return *this;
68acf21a8 (GeorgFritze     2022-06-02 10:40:51 +0200 1171)         }
6f0683bb4 (Georg Fritze    2022-05-13 13:04:17 +0200 1172)     }
```

A double will be packed as an integer when it fits, so the unit test encodes value 3.0 as
```
1-bytes blob (03)
```
instead of
```
9-bytes blob (cb 40 08 00 00 00 00 00 00)
```

However, functions on Thinknode are strongly typed, and the one the unit test tries to reach expects two doubles.

The "optimization" is not yet in version 4.1.1, and the unit test passes with that version.
