#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")"
# Keep the proven Shell loader; only reuse its deterministic RAM FAT image.
if [ ! -f build/m5/shell-disk.raw ]; then ./build-shell.sh; fi
export LC_ALL=C SOURCE_DATE_EPOCH=1791331200
LLVM_BIN=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
LLD_BIN=${LLD_BIN:-/opt/homebrew/opt/lld/bin}
python3 scripts/check-sources.py
mkdir -p build/usb/toolchain build/usb/workspace/Conf
ln -sf "$LLVM_BIN/clang" build/usb/toolchain/clang
ln -sf "$LLVM_BIN/llvm-lib" build/usb/toolchain/llvm-lib
ln -sf "$LLD_BIN/lld-link" build/usb/toolchain/lld-link
make -C third_party/edk2/BaseTools -j4 > build/usb/basetools.log 2>&1
export WORKSPACE="$PWD/build/usb/workspace" CONF_PATH="$PWD/build/usb/workspace/Conf"
export PACKAGES_PATH="$PWD:$PWD/third_party/edk2" EDK_TOOLS_PATH="$PWD/third_party/edk2/BaseTools"
export CLANG_BIN="$PWD/build/usb/toolchain/" NASM_PREFIX="$(dirname "$(command -v nasm)")/" PYTHON_COMMAND=python3
source third_party/edk2/edksetup.sh BaseTools
nasm -f bin transition/long-mode.asm -o build/usb/transition.bin
build -p edk2-platform/UsbTest.dsc -a X64 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/usb/test-build.log 2>&1
cp build/usb/workspace/Build/BaytrailUsbTest/DEBUG_CLANGPDB/X64/BaytrailUsbTest.efi build/usb/BOOTX64.EFI
build -p edk2-platform/UsbPlatform.dsc -a X64 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/usb/edk-build.log 2>&1
python3 scripts/embed-usb.py
build -p edk2-platform/UsbLoader.dsc -a IA32 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/usb/loader-build.log 2>&1
cp build/usb/workspace/Build/BaytrailUsbLoader/DEBUG_CLANGPDB/IA32/BaytrailUsbLoader.efi build/BAYTRAIL-USB-IA32.EFI
python3 scripts/verify-usb.py
python3 scripts/package-usb.py
