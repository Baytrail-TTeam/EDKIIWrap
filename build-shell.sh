#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")"
export LC_ALL=C SOURCE_DATE_EPOCH=1791331200
LLVM_BIN=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
LLD_BIN=${LLD_BIN:-/opt/homebrew/opt/lld/bin}
python3 scripts/check-sources.py
mkdir -p build/m5/toolchain build/m5/workspace/Conf
ln -sf "$LLVM_BIN/clang" build/m5/toolchain/clang
ln -sf "$LLVM_BIN/llvm-lib" build/m5/toolchain/llvm-lib
ln -sf "$LLD_BIN/lld-link" build/m5/toolchain/lld-link
make -C third_party/edk2/BaseTools -j4 > build/m5/basetools.log 2>&1
export WORKSPACE="$PWD/build/m5/workspace"
export CONF_PATH="$WORKSPACE/Conf"
export PACKAGES_PATH="$PWD:$PWD/third_party/edk2"
export EDK_TOOLS_PATH="$PWD/third_party/edk2/BaseTools"
export CLANG_BIN="$PWD/build/m5/toolchain/"
export NASM_PREFIX="$(dirname "$(command -v nasm)")/"
export PYTHON_COMMAND=python3
source third_party/edk2/edksetup.sh BaseTools
nasm -f bin transition/long-mode.asm -o build/transition.bin -l build/transition.lst
build -p edk2-platform/Shell.dsc -a X64 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/m5/shell-build.log 2>&1
python3 scripts/make-shell-disk.py
build -p edk2-platform/ShellPlatform.dsc -a X64 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/m5/edk-build.log 2>&1
python3 scripts/embed-shell.py
build -p edk2-platform/ShellLoader.dsc -a IA32 -b DEBUG -t CLANGPDB -D PROJECT_ROOT="$PWD" -n 4 > build/m5/edk-loader-build.log 2>&1
cp build/m5/workspace/Build/BaytrailShellLoader/DEBUG_CLANGPDB/IA32/BaytrailShellLoader.efi build/BAYTRAIL-SHELL-IA32.EFI
python3 scripts/verify-shell.py
