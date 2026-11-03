"""Build the standalone Slippi Invincibility Overlay Gecko code with devkitPPC.

Run: python scripts/build_invincibility_gecko.py
No ISO, external Python packages or Training Mode DAT build is required.
It shares the overlay's colors with Training Mode through src/action_cues.c.
"""
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_actionable_gecko import c2, elf_symbols, relocation_offsets, run  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "slippi-gecko-codes/invincibility-overlay"
BUILD = ROOT / "build/invincibility-gecko"
HOOK = 0x80080E18  # Fighter GX callback, first instruction (mflr r0).


def build():
    BUILD.mkdir(parents=True, exist_ok=True)
    # No jump tables: action_cues.c's single .got2 would otherwise keep the
    # unused script lookahead alive through its switch table.
    flags = ["-O2", "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750",
             "-fPIC", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections", "-fno-jump-tables",
             "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fno-stack-protector", "-w"]
    for source, output in [(SOURCE / "overlay.c", "overlay.o"), (ROOT / "src/action_cues.c", "cues.o")]:
        run("gcc", *flags, "-c", source, "-o", BUILD / output)
    run("as", "-mregnames", "-mgekko", SOURCE / "entry.S", "-o", BUILD / "entry.o")
    run("ld", "--gc-sections", "--emit-relocs", "-T", ROOT / "slippi-gecko-codes/actionable/payload.ld",
        BUILD / "entry.o", BUILD / "overlay.o", BUILD / "cues.o", "-o", BUILD / "payload.elf")
    run("objcopy", "-O", "binary", BUILD / "payload.elf", BUILD / "payload.bin")
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
    # Slippi's bootloader supports 04/06/C2, not standard Gecko conditionals.
    code = c2(HOOK, body)
    lines = ["$Invincibility Overlay (Slippi NTSC 1.02) [Tyro]",
             "*Tints invincible and intangible fighters. FFA: port colors.",
             "*Teams: each teammate gets its own shade of the team color.",
             "*Visual only. Replaces Change Colour While Invulnerable."]
    lines += [f"{a:08X} {b:08X}" for a, b in struct.iter_unpack(">II", code)]
    (SOURCE / "invincibility-overlay.txt").write_text("\n".join(lines) + "\n", encoding="ascii", newline="\n")
    (SOURCE / "invincibility-overlay.gct").write_bytes(
        bytes.fromhex("00D0C0DE 00D0C0DE") + code + bytes.fromhex("F0000000 00000000"))
    print(f"Built {len(code):,} bytes; {len(offsets)} position-independent data relocations.")
    print(SOURCE / "invincibility-overlay.txt")


if __name__ == "__main__":
    build()
