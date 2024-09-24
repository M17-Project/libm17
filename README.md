# libm17

### Overview
Written in C, it has all the components described by the protocol's specification of the stream and packet modes:
- convolutional encoder with soft Viterbi decoder (utilizing fixed point arithmetic),
- Golay encoder with soft decoder (fixed point),
- bit interleaver and randomizer,
- cyclic redundancy check (CRC) calculation (both LSF and arbitrary input),
- callsign encoder and decoder

There's no support for **any** encryption.

### Legacy Makefile building
1. Build the shared object `libm17.so` by running `make`.<br>
2. You can install the library object with `make install`.

Unit tests are available and can be run with `make test && make testrun`.

### Cmake building
1. Configre the build - run `cmake -B build` to get default options.<br>
2. Build the library by running `cmake --build build`.<br>
3. At this point tests can be run by doing any of:<br>
`cmake --build build --target test`<br>
`ctest --test-dir build`<br>
`./build/unti_tests/unit_tests`
4. Finally, installation is just `cmake --install build`
