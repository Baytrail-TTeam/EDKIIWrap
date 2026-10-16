# EDKIIWrap

EDKIIWrap is an experimental project for devices powered by Intel Bay Trail-T
processors. Its goal is to transition from stock 32-bit UEFI (IA32) to an
EDK II-based 64-bit UEFI (X64) environment, enabling these devices to boot
64-bit operating systems.

## Current status

Compatibility depends on the device and operating system. The user has confirmed
that Linux Mint boots on the target tablet. Windows boot issues remain unresolved;
successful Windows Setup tests in QEMU do not establish compatibility on hardware.

The IA32-to-X64 transition, EDK II DXE, framebuffer GOP, EFI Shell, native USB
storage and keyboard input have been confirmed on the tablet during development.
Hardware tests are performed by the user; emulator results are recorded separately.
See [STATUS.md](STATUS.md) and [USB development notes](docs/M5-USB.md) for details.
Most development documentation is currently in Polish.

Two neighboring repositories were used only as research sources.
CSMWrap and SeaBIOS are not part of this project.

## Build and test stages

- `build/BAYTRAIL-M1-IA32.EFI`: collects firmware data, displays `M1 COLLECT OK`,
  and returns after 30 seconds.
- `build/BAYTRAIL-M2-IA32.EFI`: collects the same data, calls ExitBootServices,
  installs its own page tables, enters long mode, and runs an embedded X64 payload
  that displays `STAGE 2 X64 OK` and halts the CPU.
- `build/BAYTRAIL-M3-IA32.EFI`: minimal EDK II X64 DXE, Boot Services tests,
  and ACPI/SMBIOS publication. See [M3](docs/M3.md).
- `build/BAYTRAIL-M4-IA32.EFI`: X64 GOP with QueryMode, SetMode and Blt tests,
  pixel readback, and RGB bars. See [M4](docs/M4.md).
- `build/BAYTRAIL-SHELL-IA32.EFI`: an EDK II X64 Shell with a graphical console
  and read-only FAT disk in RAM, without physical USB or keyboard support.
  See [Shell](docs/SHELL.md).
- `build/BAYTRAIL-USB-IA32.EFI`: M5 development loader with native X64 USB,
  storage, keyboard input, and interactive Shell. The USB test package also
  includes a diagnostic `build/usb/BOOTX64.EFI`.
  See [M5 USB](docs/M5-USB.md).

```sh
./build.sh        # M1/M2
./build-m3.sh     # M3
./build-m4.sh     # M4
./build-shell.sh  # X64 Shell with RAM disk
./build-usb.sh    # Native X64 USB loader and diagnostic BOOTX64.EFI
```

Toolchain: LLVM/LLD 23.1.2, NASM 3.02, and Python 3. EDK II sources
(`edk2-stable202608`) are pinned to commits in `third_party/sources.lock.json`.
See [build instructions](docs/BUILD.md).

## Validation records

- M1/M2: confirmed by user-provided hardware photos on 2026-10-07.
  See [test record](docs/MANUAL-TEST-2026-10-07.md).
- M3: verified in QEMU with IA32 OVMF and confirmed on the tablet on 2026-10-08.
  See [test record](docs/MANUAL-TEST-M3-2026-10-08.md).
- M4: confirmed on the tablet on 2026-10-08.
  See [test record](docs/MANUAL-TEST-M4-2026-10-08.md).
- Later USB, memory, Linux, and Windows investigations are documented in
  [STATUS.md](STATUS.md) and [M5 USB](docs/M5-USB.md).

## Documentation

- [Testing](docs/TESTING.md): manual procedures and failure scenarios.
- [Architecture](docs/ARCHITECTURE.md): handoff contract, mode transition,
  and integration plan.
- [Research](docs/RESEARCH.md): device research, EDK II/coreboot/MrChromebox
  sources, and limitations.
- [GOP donor metadata](docs/GOP-DONOR.json): candidate X64 GOP, hashes,
  and companion VBT information.

## Distribution boundaries

Stock firmware dumps, private ACPI captures, and device identifiers were not
copied into this repository. Downloaded third-party sources and build outputs
are excluded from Git. Redistribution rights for donor firmware have not been
established; donor firmware blobs must not be published automatically.
