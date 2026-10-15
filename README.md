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

## Boot

The kernel is linked at `0x80000` and uses UART10, retaining the baud-rate
divisors configured by the firmware. Configure the firmware console and serial
terminal for 115200 baud, 8 data bits, no parity, and 1 stop bit.
Configure the Raspberry Pi firmware to load the image at that address.
Board setup and deployment instructions are not yet documented.
