"""Build the standalone Slippi Invincibility Stoplight Gecko code with devkitPPC.

Run: python scripts/build_invincibility_stoplight_gecko.py
No ISO, external Python packages or Training Mode DAT build is required.
It shares its countdown and colors with Training Mode through src/action_cues.c.
"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_invincibility_gecko import ROOT, build_code  # noqa: E402

SOURCE = ROOT / "slippi-gecko-codes/invincibility-stoplight"
BUILD = ROOT / "build/invincibility-stoplight-gecko"
# The fighter GX callback's second instruction (stw r0,4(r1)), so the
# Invincibility Overlay code can keep its hook on the first.
HOOK = 0x80080E1C


def build():
    build_code(SOURCE, BUILD, HOOK, [
        "$Invincibility Stoplight (Slippi NTSC 1.02) [Tyro]",
        "*Last 4 invincible frames: red, red, then yellow, yellow.",
        "*Same colors for everyone. Works with or without Invincibility Overlay.",
        "*Visual only."], "invincibility-stoplight")


if __name__ == "__main__":
    build()
