#!/usr/bin/env python3
"""Slim a PIC32 bootloader-app UF2: truncate the raw binary to the actually-used
program-flash span (4KB-page aligned, always below the reserved NVM page) before
UF2 conversion.

Usage: uf2_slim.py <objdump> <elf> <bin> <uf2conv.py> <uf2-out> <base-addr>

Physical map (cutcutgo _bl config):
  UF2 base       0x1D010000 (== KVA 0x9D010000, app reset vector)
  app region end 0x9D07F000 (reserved NVM page start — never covered)
"""
import subprocess
import sys

BASE_KVA = 0x9D010000
NVM_PAGE_KVA = 0x9D07F000
PAGE = 0x1000


def main() -> int:
    objdump, elf, binfile, uf2conv, uf2out, base_arg = sys.argv[1:7]
    base = int(base_arg, 16)
    assert base == BASE_KVA - 0x80000000, f"unexpected UF2 base {base_arg}"

    sections = subprocess.run([objdump, "-h", elf], capture_output=True, text=True, check=True).stdout
    max_end = 0
    for line in sections.splitlines():
        parts = line.split()
        # Idx Name Size VMA LMA File off  Algn  (xc32-objdump prints hex WITHOUT 0x)
        if len(parts) >= 6 and parts[0].isdigit():
            try:
                vma = int(parts[3], 16)
                size = int(parts[2], 16)
            except ValueError:
                continue
            if BASE_KVA <= vma < NVM_PAGE_KVA:
                max_end = max(max_end, vma + size)
    if max_end == 0:
        print("ERROR: no program-flash sections found — refusing to build an empty UF2", file=sys.stderr)
        return 1
    span = ((max_end - BASE_KVA) + PAGE - 1) // PAGE * PAGE
    if span >= NVM_PAGE_KVA - BASE_KVA:
        print("ERROR: computed span reaches the NVM page — aborting", file=sys.stderr)
        return 1

    with open(binfile, "r+b") as f:
        f.truncate(span)

    subprocess.run(
        [sys.executable, uf2conv, "-c", "-f", "0x4d414b52", "-b", base_arg, "-o", uf2out, binfile],
        check=True,
    )
    print(f"uf2_slim: code end KVA 0x{max_end:X} -> span {span} bytes ({span // 1024} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
