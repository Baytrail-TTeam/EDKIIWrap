#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")"
export LC_ALL=C SOURCE_DATE_EPOCH=1791331200
LLVM_BIN=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
LLD_BIN=${LLD_BIN:-/opt/homebrew/opt/lld/bin}
python3 scripts/check-sources.py
mkdir -p build/m3/toolchain build/m3/workspace/Conf
ln -sf "$LLVM_BIN/clang" build/m3/toolchain/clang
ln -sf "$LLVM_BIN/llvm-lib" build/m3/toolchain/llvm-lib
ln -sf "$LLD_BIN/lld-link" build/m3/toolchain/lld-link
make -C third_party/edk2/BaseTools -j4 > build/m3/basetools.log 2>&1
export WORKSPACE="$PWD/build/m3/workspace"
export CONF_PATH="$WORKSPACE/Conf"
export PACKAGES_PATH="$PWD:$PWD/third_party/edk2"
export EDK_TOOLS_PATH="$PWD/third_party/edk2/BaseTools"
export CLANG_BIN="$PWD/build/m3/toolchain/"
export NASM_PREFIX="$(dirname "$(command -v nasm)")/"
export PYTHON_COMMAND=python3
source third_party/edk2/edksetup.sh BaseTools
nasm -f bin transition/long-mode.asm -o build/transition.bin -l build/transition.lst
build -p edk2-platform/M3.dsc -a X64 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/m3/edk-build.log 2>&1
python3 scripts/embed-m3.py
build -p edk2-platform/M3Loader.dsc -a IA32 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/m3/edk-loader-build.log 2>&1
cp build/m3/workspace/Build/BaytrailM3Loader/DEBUG_CLANGPDB/IA32/BaytrailM3Loader.efi build/BAYTRAIL-M3-IA32.EFI
python3 scripts/verify-m3.py
