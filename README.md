# EDKIIWrap

I am developing EDKIIWrap for devices powered by Intel Bay Trail-T processors.
My goal is to transition from stock 32-bit UEFI (IA32) to an EDK II-based
64-bit UEFI (X64) environment so these devices can boot 64-bit operating systems.

This is an experimental project. Compatibility depends on the device and
operating system.

## Current status

On my tablet, I can run the X64 EFI Shell, read USB storage, and use a USB
keyboard. My current operating system boot results are:

| Operating system | Result on my tablet |
| --- | --- |
| Linux Mint | Boots successfully. |
| Windows 10 X64 | Stops at a blue screen without an error code. |
| Windows 7 X64 | Freezes at **Starting Windows**, before the installer appears. |

I have not resolved the Windows boot issues. Successful emulator runs do not
establish compatibility on the tablet.

## Build

```sh
./build-usb.sh
```

The current loader is built as `build/BAYTRAIL-USB-IA32.EFI`. It provides an
EDK II X64 environment with native USB storage, keyboard input, and an
interactive EFI Shell, where I can select an X64 boot application manually.

The build also produces a diagnostic `BOOTX64.EFI`; this is not an operating
system bootloader and should not replace Ventoy's `BOOTX64.EFI`.

Toolchain: LLVM/LLD 23.1.2, NASM 3.02, and Python 3. EDK II sources
(`edk2-stable202608`) are pinned to commits in `third_party/sources.lock.json`.
See [build instructions](docs/BUILD.md) for setup details.

## Technical notes

I keep implementation details and investigation records in
[the USB development notes](docs/M5-USB.md). Most development documentation is
currently in Polish and includes historical results that may not reflect the
current hardware status above.

CSMWrap and SeaBIOS are not part of EDKIIWrap.

## Distribution boundaries

I do not include stock firmware dumps, private ACPI captures, or device
identifiers in this repository. Downloaded third-party sources and build outputs
are excluded from Git. Redistribution rights for donor firmware have not been
established, so I do not publish those firmware blobs.
