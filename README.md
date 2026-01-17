# libm17

### Overview
**Libm17** is a C implementation of the [M17 protocol's](https://en.wikipedia.org/wiki/M17_(amateur_radio)) RF stack, as described by its [specification document](https://spec.m17project.org).

The library includes:
- soft symbol slicer (RX) and symbol mapper (TX),
- convolutional encoder with soft Viterbi decoder (utilizing fixed point arithmetic),
- Golay encoder with soft decoder (fixed point arithmetic),
- bit interleaver and randomizer,
- cyclic redundancy check (CRC) calculation (LSF/LSD and arbitrary input),
- callsign encoder and decoder,
- LSF/LSD META field extended callsign data, cryptographic nonce, and GNSS position data encoders/decoders,
- Root Raised Cosine (RRC) filter taps (for 24kHz and 48kHz sample rates) - linear and polyphase.

There's no support for **any** encryption within the library - it has to be handled by the developer, using own code.

### Address encoding convention
As per the [specification document](https://github.com/M17-Project/M17_spec), the address space is divided into parts. Encodable callsign space is accessed by using the base40 alphabet. If the first character is set to `#`, the address belongs to the extended hash-address space. The broadcast address is represented by `@ALL`. Zero-address is invalid and decodes into a null-string. The reserved chunk for application use cannot be accessed using any string.

### Legacy Makefile building
1. Build the shared object `libm17.so` by running `make`.<br>
2. Optionally, the library object can be installed with `make install`.

Unit tests are available and can be compiled and run with `make test && make testrun`.

### Cmake building
1. Configure the build - run `cmake -B build` to get default options.<br>
2. Build the library by running `cmake --build build`.<br>
3. At this point tests can be run by doing any of:<br>
* `cmake --build build --target test`<br>
* `ctest --test-dir build`<br>
* `./build/unit_tests/unit_tests`
4. Finally, installation is just `sudo cmake --install build`

### Unit tests
Unit tests use the [Unity](https://github.com/ThrowTheSwitch/Unity) unit testing framework.
