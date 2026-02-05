# libyang example

This example demonstrates using libyang v2 C API to:

- load YANG modules from `/usr/local/share/yang/modules`;
- programmatically build a `ietf-network-instance` data tree with a `routing` mount (using `ietf-routing` if present);
- validate the tree with libyang;
- print the resulting data as XML using libyang serialization.

Prerequisites
- libyang v2 installed with headers under `/usr/local/include/libyang` and library under `/usr/local/lib`.
- YANG modules including `ietf-network-instance@2019-01-21.yang` and `ietf-routing.yang` (and dependencies) present in `/usr/local/share/yang/modules`.

Build
```sh
make
```

Run
```sh
./example
```
