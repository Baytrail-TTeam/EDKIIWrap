#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
export SOURCE_DATE_EPOCH=1791331200
LLVM_BIN=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
LLD_BIN=${LLD_BIN:-/opt/homebrew/opt/lld/bin}
CC=${CC:-$LLVM_BIN/clang}
OBJCOPY=${OBJCOPY:-$LLVM_BIN/llvm-objcopy}
mkdir -p build
python3 scripts/check-sources.py
nasm -f bin transition/long-mode.asm -o build/transition.bin -l build/transition.lst
common=(-Os -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident -Wall -Wextra -Werror -Iinclude)
"$CC" --target=x86_64-none-elf "${common[@]}" -fpie -mno-red-zone -mgeneral-regs-only -c payload-x64/payload.c -o build/payload.o
"$LLD_BIN/ld.lld" -T payload-x64/payload.ld --no-undefined build/payload.o -o build/payload.elf
"$OBJCOPY" -O binary build/payload.elf build/payload.bin
python3 scripts/embed.py
for stage in 1 2; do
 "$CC" --target=i686-pc-windows-msvc "${common[@]}" -fshort-wchar -mno-sse -mno-mmx \
  -Ithird_party/edk2/MdePkg/Include -Ithird_party/edk2/MdePkg/Include/Ia32 -Ibuild \
  -DRUN_TRANSITION=$((stage-1)) -c loader-ia32/main.c -o "build/loader-m$stage.obj"
 "$LLD_BIN/lld-link" /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x86 \
  /safeseh:no /dynamicbase /fixed:no /timestamp:0 /filealign:512 \
  "/out:build/BAYTRAIL-M$stage-IA32.EFI" "build/loader-m$stage.obj"
done
"$LLVM_BIN/llvm-readobj" --file-headers --coff-basereloc build/BAYTRAIL-M2-IA32.EFI > build/pe-report.txt
"$LLVM_BIN/llvm-objdump" -d build/payload.elf > build/payload-disassembly.txt
"$LLVM_BIN/llvm-readobj" --relocations --symbols build/payload.elf > build/payload-report.txt
python3 scripts/verify.py
python3 scripts/manifest.py
