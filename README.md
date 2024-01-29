# libm17

### Overview
Written in C, it has all the components described by the protocol's specification of the stream and packet modes:
- convolutional encoder with soft Viterbi decoder (utilizing fixed point arithmetic),
- Golay encoder with soft decoder (fixed point),
- bit interleaver and randomizer,
- cyclic redundancy check (CRC) calculation (both LSF and arbitrary input),
- callsign encoder and decoder

There's no support for **any** encryption.

### Legacy Makefile Building
First, build the shared object `libm17.so` by running `make`.
Then, you can install the library object with `make install`.
Unit tests are available and can be run with `make test && make testrun`.

### Cmake building
First, configre the build. Do `cmake -B build` to get default options.
The build the library by running `cmake --build build`.
At this point tests can be run by doing any of:
`cmake --build build --target test`
`ctest --test-dir build`
`./build/unti_tests/unit_tests`
Finally, installation is just `cmake --install build`
