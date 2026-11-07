# RaspPiOs

A small bare-metal AArch64 kernel for the Raspberry Pi 5.

## Current features

- EL1 kernel startup and exception handling
- UART10 serial output
- GIC interrupt handling and physical timer ticks
- Basic kernel printing and string utilities

## Build

Install an AArch64 bare-metal GCC toolchain providing `aarch64-elf-gcc` and
`aarch64-elf-objcopy`, then run:

```sh
make img
```

This produces `kernel.elf` and the raw kernel image `myos.img`.

To remove generated build files:

```sh
make clean
```

## Tests

Host unit tests use [Embedded Test (ET)](https://github.com/QuantumLeaps/Embedded-Test),
vendored in `tests/framework/Embedded-Test`. Run:

```sh
make host-tests
```

Tests run with the host C compiler (`HOST_CC`, default `cc`), independently of
the AArch64 kernel build. `make host-tests` (or `make test`) builds separate
executables for the conversion and FDT suites, then runs every executable,
returning failure if any suite fails. Run from the project root so the FDT suite
can open `bcm2712-rpi-b.dtb`.

To add a suite, add its filename without `.c` to `HOST_TEST_NAMES` in the
Makefile and list any implementation sources and headers as prerequisites of
its executable. The empty memory-map suite is not registered yet.

ET's upstream revision and update instructions are recorded in
`tests/framework/Embedded-Test/UPSTREAM.md`.

Each ET executable contains one `TEST_GROUP`, with `TEST` blocks and `VERIFY`
assertions, plus `setup()` and `teardown()` callbacks. ET provides `main()`;
`et_host.c` supplies console output and process exit. A failed assertion stops
the run immediately with a nonzero exit status.

For future on-device tests, replace `et_host.c` with a Pi adapter implementing
`ET_onInit`, `ET_onPrintChar`, and a non-returning `ET_onExit`. The kernel startup
must invoke ET's entry point after the required hardware initialization.

## Boot

The kernel is linked at `0x80000` and uses UART10, retaining the baud-rate
divisors configured by the firmware. Configure the firmware console and serial
terminal for 115200 baud, 8 data bits, no parity, and 1 stop bit.
Configure the Raspberry Pi firmware to load the image at that address.
Board setup and deployment instructions are not yet documented.
