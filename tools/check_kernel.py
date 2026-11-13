#!/usr/bin/env python3
"""Validate the basic ELF invariants required by the Arechi x86-64 boot path."""

import struct
import sys
from pathlib import Path

ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIQQQQQQ")

PT_LOAD = 1
PF_X = 1
PF_W = 2
EM_X86_64 = 62
ET_EXEC = 2
KERNEL_BASE = 0xFFFFFFFF80000000


def fail(message: str) -> None:
    raise SystemExit(f"kernel check failed: {message}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: check_kernel.py <kernel.elf>")

    path = Path(sys.argv[1])
    try:
        image = path.read_bytes()
    except OSError as error:
        fail(f"cannot read {path}: {error}")

    if len(image) < ELF_HEADER.size:
        fail("file is too small to contain an ELF64 header")

    header = ELF_HEADER.unpack_from(image)
    ident = header[0]
    elf_type, machine = header[1], header[2]
    entry, phoff = header[4], header[5]
    ehsize, phentsize, phnum = header[8], header[9], header[10]

    if ident[:4] != b"\x7fELF":
        fail("invalid ELF magic")
    if ident[4] != 2 or ident[5] != 1:
        fail("expected a little-endian ELF64 image")
    if elf_type != ET_EXEC or machine != EM_X86_64:
        fail("expected an x86-64 executable (ET_EXEC)")
    if ehsize != ELF_HEADER.size or phentsize < PROGRAM_HEADER.size:
        fail("unexpected ELF header or program-header size")
    if phnum == 0 or phoff + phentsize * phnum > len(image):
        fail("invalid program-header table")
    if entry < KERNEL_BASE:
        fail(f"entry point 0x{entry:016x} is not in the high-half kernel")

    executable_entry = False
    load_segments = 0

    for index in range(phnum):
        offset = phoff + index * phentsize
        segment = PROGRAM_HEADER.unpack_from(image, offset)
        p_type, flags = segment[0], segment[1]
        p_offset, p_vaddr, p_paddr = segment[2], segment[3], segment[4]
        p_filesz, p_memsz = segment[5], segment[6]

        if p_type != PT_LOAD:
            continue

        load_segments += 1
        if p_offset + p_filesz > len(image):
            fail(f"load segment {index} extends beyond the file")
        if p_memsz < p_filesz:
            fail(f"load segment {index} has p_memsz smaller than p_filesz")
        if flags & PF_W and flags & PF_X:
            fail(f"load segment {index} is both writable and executable")
        if flags & PF_X and p_vaddr <= entry < p_vaddr + p_memsz:
            executable_entry = True
        if p_vaddr < KERNEL_BASE or p_paddr == 0:
            fail(f"load segment {index} has an unexpected address")

    if load_segments == 0:
        fail("no PT_LOAD segments found")
    if not executable_entry:
        fail("entry point is not inside an executable load segment")

    print("ELF64/x86-64 high-half executable: OK")
    print(f"Entry point: 0x{entry:016x}")
    print(f"Loadable segments: {load_segments}")
    print("W^X segment permissions: OK")


if __name__ == "__main__":
    main()
