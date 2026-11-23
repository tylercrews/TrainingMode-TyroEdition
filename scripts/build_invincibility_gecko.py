"""Build the standalone Slippi Invincibility Overlay Gecko code with devkitPPC.

Run: python scripts/build_invincibility_gecko.py
No ISO, external Python packages or Training Mode DAT build is required.
It shares the overlay's colors with Training Mode through src/action_cues.c.
build_code also builds the Invincibility Stoplight (build_invincibility_stoplight_gecko.py).
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


def build_code(source, build, hook, lines, stem):
    """Link source's entry.S and overlay.c with src/action_cues.c into one C2 code."""
    build.mkdir(parents=True, exist_ok=True)
    # No jump tables: action_cues.c's single .got2 would otherwise keep the
    # script lookahead's switch table, needing a relocation per case.
    flags = ["-O2", "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750",
             "-fPIC", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections", "-fno-jump-tables",
             "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-fno-stack-protector", "-w"]
    for path, output in [(source / "overlay.c", "overlay.o"), (ROOT / "src/action_cues.c", "cues.o")]:
        run("gcc", *flags, "-c", path, "-o", build / output)
    run("as", "-mregnames", "-mgekko", source / "entry.S", "-o", build / "entry.o")
    run("ld", "--gc-sections", "--emit-relocs", "-T", ROOT / "slippi-gecko-codes/actionable/payload.ld",
        build / "entry.o", build / "overlay.o", build / "cues.o", "-o", build / "payload.elf")
    run("objcopy", "-O", "binary", build / "payload.elf", build / "payload.bin")
    elf = (build / "payload.elf").read_bytes()
    symbols = elf_symbols(elf)
    offsets = relocation_offsets(elf)
    body = bytearray((build / "payload.bin").read_bytes())
    for at in offsets:
        pointer = struct.unpack_from(">I", body, at)[0]
        if pointer >= len(body):
            raise ValueError(f"Nonlocal relocation at {at:x}: {pointer:x}")
    struct.pack_into(">I", body, symbols["reloc_offset"], len(body))
    struct.pack_into(">I", body, symbols["reloc_count"], len(offsets))
    body += b"".join(struct.pack(">I", at) for at in offsets)
    # Slippi's bootloader supports 04/06/C2, not standard Gecko conditionals.
    code = c2(hook, body)
    lines = lines + [f"{a:08X} {b:08X}" for a, b in struct.iter_unpack(">II", code)]
    (source / f"{stem}.txt").write_text("\n".join(lines) + "\n", encoding="ascii", newline="\n")
    (source / f"{stem}.gct").write_bytes(
        bytes.fromhex("00D0C0DE 00D0C0DE") + code + bytes.fromhex("F0000000 00000000"))
    print(f"Built {len(code):,} bytes; {len(offsets)} position-independent data relocations.")
    print(source / f"{stem}.txt")


def build():
    build_code(SOURCE, BUILD, HOOK, [
        "$Invincibility Overlay (Slippi NTSC 1.02) [Tyro]",
        "*Tints invincible and intangible fighters. FFA: port colors.",
        "*Teams: each teammate gets its own shade of the team color.",
        "*Visual only. Replaces Change Colour While Invulnerable."], "invincibility-overlay")


if __name__ == "__main__":
    build()
