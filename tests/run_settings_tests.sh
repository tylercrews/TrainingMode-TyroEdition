#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
mkdir -p build
# Compile a fresh optimized production DAT too: ELF linking does not reproduce
# hmex/native-loader relocation limitations (including symbol address addends).
case "$(uname)" in
    MSYS*|MINGW*) settings_hmex=bin/hmex.exe; settings_hgecko=bin/hgecko.exe ;;
    Darwin*) settings_hmex=bin/hmex_macos_arm64; settings_hgecko=bin/hgecko_macos_arm64 ;;
    *) settings_hmex=bin/hmex; settings_hgecko=bin/hgecko ;;
esac
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s tmFunction \
    -t MexTK/tmFunction.txt -o build/settings-relocated-test.dat \
    -i src/events.c src/menu.c src/osds.c src/savestate_v1.c src/settings.c src/settings_extension.c src/settings_game.c \
       src/trails.c src/trails_game.c src/osd_context.c src/osd_context_game.c src/osd_style.c src/osd_style_game.c src/osd_layout.c src/osd_layout_game.c src/osd_editor_game.c \
       src/powershield_osd_game.c src/action_cues.c src/action_cues_game.c src/asmash_osd_game.c -dat dats/eventMenu.dat
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s evFunction \
    -t MexTK/evFunction.txt -o build/sweetspot-reset-test.dat -i src/sweetspot.c
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s evFunction \
    -t MexTK/evFunction.txt -o build/ledgechase-test.dat -i src/ledgechase.c
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s evFunction \
    -t MexTK/evFunction.txt -o build/ledgedash-trails-test.dat -i src/ledgedash.c src/ledgedash_logic.c
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s evFunction \
    -t MexTK/evFunction.txt -o build/lowlaser-test.dat -i src/lowlaser.c
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s evFunction \
    -t MexTK/evFunction.txt -o build/powershield-test.dat -i src/powershield.c
powerpc-eabi-gcc -O2 -ffreestanding -fno-builtin -msdata=none -mcpu=750 \
    -nostdlib -Wa,-mregnames -Wl,-Ttext=0x101000,-e,TMSettings_Init \
    src/settings.c src/settings_extension.c src/settings_game.c src/trails.c src/trails_game.c src/osd_context.c src/osd_context_game.c src/osd_style.c src/osd_style_game.c src/osd_layout.c src/osd_layout_game.c src/osd_editor_game.c src/powershield_osd_game.c src/action_cues.c src/action_cues_game.c src/asmash_osd_game.c src/ledgedash_logic.c tests/settings_support.c tests/settings_hooks.S \
    -o build/settings-test.elf
"${settings_hgecko}" -q ASM build/settings-codes-test.gct
# Keep labels for executing the actual Under Fire think/restart assembly.
(cd 'ASM/training-mode/Custom Events' && powerpc-eabi-as -mregnames \
    'Custom Event Code - Rewrite.asm' -o ../../../build/under-fire-native-test.o)
powerpc-eabi-ld -Ttext=0x80480000 -e 0 build/under-fire-native-test.o -o build/under-fire-native-test.elf
python tests/test_settings.py -v
python tests/test_stick_hud.py -v
python tests/test_settings_extension.py -v
python tests/test_score_storage.py -v
python tests/test_asmash.py -v
python tests/test_techchase.py -v
python tests/test_cpu_overlay.py -v
python tests/test_ledgechase_catalogue.py -v
python tests/test_ledgechase.py -v
python tests/test_event_trails.py -v
python tests/test_under_fire.py -v
python tests/test_stadium.py -v
python tests/test_lowlaser.py -v
python tests/test_powershield.py -v
