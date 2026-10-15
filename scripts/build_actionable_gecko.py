"""Build a standalone Slippi Gecko code with the installed devkitPPC tools.

Run: python scripts/build_actionable_gecko.py
No ISO, external Python packages or Training Mode DAT build is required.
"""
from pathlib import Path
import os
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "gecko/actionable"
BUILD = ROOT / "build/actionable-gecko"


def tool(name):
    found = shutil.which("powerpc-eabi-" + name)
    if found:
        return found
    for base in [os.environ.get("DEVKITPPC", ""), "C:/devkitPro/devkitPPC", "/opt/devkitpro/devkitPPC"]:
        for suffix in [".exe", ""]:
            candidate = Path(base) / "bin" / ("powerpc-eabi-" + name + suffix)
            if candidate.is_file():
                return str(candidate)
    raise SystemExit("Install devkitPPC or add its bin directory to PATH.")


def run(name, *args):
    subprocess.run([tool(name), *map(str, args)], check=True, cwd=ROOT)


def elf_sections(data):
    assert data[:6] == b"\x7fELF\x01\x02", "Expected big-endian ELF32"
    offset = struct.unpack_from(">I", data, 0x20)[0]
    stride, count = struct.unpack_from(">HH", data, 0x2E)
    return [struct.unpack_from(">10I", data, offset + i * stride) for i in range(count)]


def elf_symbols(data):
    sections = elf_sections(data)
    symbols = {}
    for section in sections:
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for at in range(section[4], section[4] + section[5], section[9]):
            name, value, _, _, _, _ = struct.unpack_from(">IIIBBH", data, at)
            symbols[names[name:].split(b"\0", 1)[0].decode()] = value
    return symbols


def relocation_offsets(data):
    sections = elf_sections(data)
    result = []
    for section in sections:
        if section[1] != 4 or not sections[section[7]][2] & 2:
            continue
        for at in range(section[4], section[4] + section[5], section[9]):
            offset, info, _ = struct.unpack_from(">IIi", data, at)
            kind = info & 255
            if kind == 1:  # R_PPC_ADDR32: local pointer in GOT/data
                result.append(offset)
            elif kind not in (10, 18, 26):  # REL24 / resolved PLTREL24 / REL32
                raise ValueError(f"Unsupported PPC relocation {kind} at {offset:x}")
    return sorted(set(result))


def c2(address, body):
    body = bytes(body)
    if len(body) % 4:
        raise ValueError("Unaligned C2 body")
    # Final word is reserved for the Gecko handler's return branch.
    if len(body) % 8 == 0:
        body += bytes.fromhex("60000000")
    body += b"\0" * 4
    return struct.pack(">II", 0xC2000000 | (address & 0x1FFFFFF), len(body) // 8) + body


def build():
    BUILD.mkdir(parents=True, exist_ok=True)
    flags = ["-O2", "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750",
             "-fPIC", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections",
             "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fno-stack-protector", "-w"]
    for source, output in [(SOURCE / "overlay.c", "overlay.o"), (ROOT / "src/action_cues.c", "cues.o")]:
        run("gcc", *flags, "-c", source, "-o", BUILD / output)
    for name in ["entry", "tick"]:
        run("as", "-mregnames", "-mgekko", SOURCE / (name + ".S"), "-o", BUILD / (name + ".o"))
    run("ld", "--gc-sections", "--emit-relocs", "-T", SOURCE / "payload.ld",
        BUILD / "entry.o", BUILD / "overlay.o", BUILD / "cues.o", "-o", BUILD / "payload.elf")
    run("objcopy", "-O", "binary", BUILD / "payload.elf", BUILD / "payload.bin")
    run("objcopy", "-O", "binary", "-j", ".text", BUILD / "tick.o", BUILD / "tick.bin")
    elf = (BUILD / "payload.elf").read_bytes()
    symbols = elf_symbols(elf)
    offsets = relocation_offsets(elf)
    body = bytearray((BUILD / "payload.bin").read_bytes())
    for at in offsets:
        pointer = struct.unpack_from(">I", body, at)[0]
        if pointer >= len(body):
            raise ValueError(f"Nonlocal relocation at {at:x}: {pointer:x}")
    struct.pack_into(">I", body, symbols["reloc_offset"], len(body))
    struct.pack_into(">I", body, symbols["reloc_count"], len(offsets))
    body += b"".join(struct.pack(">I", at) for at in offsets)
    (BUILD / "relocated-payload.bin").write_bytes(body)
    # Slippi's bootloader supports 04/06/C2, not standard Gecko conditionals.
    code = c2(0x800805C8, body)
    code += c2(0x8006D9EC, (BUILD / "tick.bin").read_bytes())
    title = "$Actionable Yellow-Green (Slippi NTSC 1.02) [Tyro]"
    lines = [title, "*Two yellow recovery frames, then two green completion frames.",
             "*For live Slippi and offline VS with its standard required codes enabled.",
             "*Applies to all fighters. Replay file saving may be disabled."]
    lines += [f"{a:08X} {b:08X}" for a, b in struct.iter_unpack(">II", code)]
    (SOURCE / "actionable.txt").write_text("\n".join(lines) + "\n", encoding="ascii")
    (SOURCE / "actionable.gct").write_bytes(bytes.fromhex("00D0C0DE 00D0C0DE") + code + bytes.fromhex("F0000000 00000000"))
    print(f"Built {len(code):,} bytes; {len(offsets)} position-independent data relocations.")
    print(SOURCE / "actionable.txt")


if __name__ == "__main__":
    build()
