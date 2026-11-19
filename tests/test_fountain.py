"""Run the Lab's Fountain of Dreams platform holds against the native platform think.

Each frame runs the DOL's Stages_FoDPlatformThink on both side platforms, then
the Lab's Fountain_Apply, the order the game runs the stage and Event_PostThink.
GrIz.dat's real platform parameters come from melee.iso. Joints, the GObj list,
the collision update and the platform animations are fixtures.
"""
from pathlib import Path
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from test_score_storage import NativeMachine
from unicorn import ppc_const as reg

THINK = 0x801CC358              # Stages_FoDPlatformThink
PARAM_POINTER = 0x804D6968      # r13 - 19768, grIzumi's yakumono_param
PARAMS = 0x805D0000
HIDDEN = 0x10                   # JOBJ_HIDDEN
WAIT, MOVE, GONE = 1, 2, 4
LEFT, RIGHT, FOUNTAIN, LOOKALIKE = range(4)
RANDOM, HIDE, FIRST = 0, 1, 2


def build():
    subprocess.run([
        "C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe", "-O2", "-w",
        "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-Wl,-Ttext=0x80600000,-e,TestFountainInit,--unresolved-symbols=ignore-all,"
        "--defsym,JOBJ_SetFlagsAll=0x80371D9C,--defsym,JOBJ_ClearFlagsAll=0x80371F9C,"
        "--defsym,JOBJ_SetMtxDirtySub=0x803732E8",
        "tests/fountain_probe.c", "-o", "build/fountain-test.elf"
    ], cwd=ROOT, check=True)


def iso_file(filename):
    with (ROOT / "melee.iso").open("rb") as iso:
        iso.seek(0x424)
        offset, size = struct.unpack(">II", iso.read(8))
        iso.seek(offset)
        fst = iso.read(size)
        count = struct.unpack_from(">I", fst, 8)[0]
        names = fst[12 * count:]
        for index in range(1, count):
            name, at, length = struct.unpack_from(">3I", fst, 12 * index)
            if names[name & 0xFFFFFF:].split(b"\0", 1)[0].decode() == filename:
                iso.seek(at)
                return iso.read(length)
    raise AssertionError(filename + " missing from melee.iso")


def dat_roots(dat):
    _, size, relocs, publics, externs = struct.unpack_from(">5I", dat)
    roots_at = 32 + size + 4 * relocs
    strings = roots_at + 8 * (publics + externs)
    roots = {}
    for index in range(publics):
        data, name = struct.unpack_from(">II", dat, roots_at + index * 8)
        roots[dat[strings + name:].split(b"\0", 1)[0].decode()] = 32 + data
    return roots


DAT = iso_file("GrIz.dat")
ROOTS = dat_roots(DAT)
YAKUMONO = DAT[ROOTS["yakumono_param"]:ROOTS["yakumono_param"] + 0x58]


def param(offset):
    return struct.unpack_from(">f", YAKUMONO, offset)[0]


class Machine(NativeMachine):
    def __init__(self, stage=2):
        super().__init__()
        elf = (ROOT / "build/fountain-test.elf").read_bytes()
        offset = struct.unpack_from(">I", elf, 0x20)[0]
        stride, count = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, offset + i * stride) for i in range(count)]
        self.symbols = {}
        for section in sections:
            _, kind, flags, address, at, size, link, _, _, entry = section
            if flags & 2 and size and kind != 8:
                self.cpu.mem_write(address, elf[at:at + size])
            if kind == 2:
                strings = sections[link]
                names = elf[strings[4]:strings[4] + strings[5]]
                for index in range(0, size, entry):
                    name, value, *_ = struct.unpack_from(">IIIBBH", elf, at + index)
                    self.symbols[names[name:].split(b"\0", 1)[0].decode()] = value
        self.cpu.mem_write(PARAMS, YAKUMONO)
        self.cpu.mem_write(PARAM_POINTER, struct.pack(">I", PARAMS))
        # Stage_MovingCollisionPointsUpdate, __assert, JOBJ_RemoveAnimAll, Stage_SetMapJOBJAnim
        for native, probe in ((0x80055E9C, "TestCollision"), (0x80388220, "TestAssert"),
                              (0x8036F6B4, "JOBJ_RemoveAnimAll"), (0x801C7FF8, "Stage_SetMapJOBJAnim")):
            self.jump(native, self.symbols[probe])
        self.layout = [self.fn("TestFountainLayout", i) for i in range(8)]
        self.stage = stage
        self.fn("TestFountainInit", stage)

    def jump(self, at, target):
        self.cpu.mem_write(at, struct.pack(">4I", 0x3D800000 | target >> 16,
                            0x618C0000 | target & 0xFFFF, 0x7D8903A6, 0x4E800420))

    def fn(self, name, *args):
        return self.run(self.symbols[name], *args, name=name)

    def run(self, address, *args, name=None):
        try:
            return self.call(address, *args)
        except Exception as error:
            pc = self.cpu.reg_read(reg.UC_PPC_REG_PC)
            raise AssertionError(f"{name or hex(address)} failed at {pc:08X}, instruction "
                                 f"{bytes(self.cpu.mem_read(pc, 4)).hex()}") from error

    def info(self, what):
        value = self.fn("TestFountainInfo", what)
        return value - (1 << 32) if value & 0x80000000 else value

    def string(self, address):
        return bytes(self.cpu.mem_read(address, 16)).split(b"\0", 1)[0].decode()

    def side_bytes(self, index):
        return bytes(self.cpu.mem_read(self.fn("TestFountainSide", index), self.layout[4]))

    def side(self, index):
        base = self.fn("TestFountainSide", index)
        data, column, water, joint = (base + self.layout[i] for i in range(4))
        flags, scale, trans = self.layout[5:8]
        f = lambda at: struct.unpack(">f", self.cpu.mem_read(at, 4))[0]
        word = lambda at: struct.unpack(">I", self.cpu.mem_read(at, 4))[0]
        state, timer = struct.unpack(">hh", self.cpu.mem_read(data + 0xC4, 4))
        return {
            "state": state, "timer": timer, "height": f(data + 0xD0), "target": f(data + 0xD4),
            "joint_y": f(joint + trans + 4), "joint_flags": word(joint + flags),
            "column_flags": word(column + flags), "water_flags": word(water + flags),
            "water_scale": (f(water + scale), f(water + scale + 4)),
        }

    def set_side(self, index, state=None, timer=None, height=None, target=None):
        data = self.fn("TestFountainSide", index) + self.layout[0]
        if state is not None:
            self.cpu.mem_write(data + 0xC4, struct.pack(">h", state))
        if timer is not None:
            self.cpu.mem_write(data + 0xC6, struct.pack(">h", timer))
        if height is not None:
            self.cpu.mem_write(data + 0xD0, struct.pack(">f", height))
        if target is not None:
            self.cpu.mem_write(data + 0xD4, struct.pack(">f", target))

    def think(self):
        for index in (LEFT, RIGHT):
            self.run(THINK, self.fn("TestFountainGObj", index), name="Stages_FoDPlatformThink")

    def frame(self, left=None, right=RANDOM):
        """One game frame: the stage's think, then the Lab's (None: no Lab)."""
        self.think()
        if left is not None:
            self.fn("TestFountainApply", left & 0xFFFFFFFF, right & 0xFFFFFFFF)

    def sink_natively(self, index):
        """Let the game sink a platform from just above its sink depth."""
        self.set_side(index, state=MOVE, height=0.25, target=-1.0)
        for _ in range(30):
            self.frame()
            if self.side(index)["state"] == GONE:
                return self.side(index)
        raise AssertionError("The platform never sank")


def joint_height(choice):
    return 15 + 0.5 * (choice - FIRST)


class FountainTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build()

    def assertClose(self, a, b, places=5):
        self.assertAlmostEqual(a, b, places=places)

    def test_choices_cover_the_games_heights_in_stage_units(self):
        m = Machine()
        count = m.info(5)
        labels = [m.string(m.fn("TestFountainLabel", i)) for i in range(count)]
        self.assertEqual(labels[:2], ["Random", "Hidden"])
        heights = []
        for choice in range(FIRST, count):
            m.fn("TestFountainApply", choice, RANDOM)
            heights.append(m.side(LEFT)["joint_y"])
        # The native think clamps every move to [0x24, 0x20], and starts the
        # left at 0x0, the right at 0x8 and both resurface at 0xC.
        self.assertEqual((heights[0], heights[-1]), (param(0x24), param(0x20)))
        self.assertEqual(heights, [joint_height(c) for c in range(FIRST, count)])
        for start in (0x0, 0x8, 0xC):
            self.assertIn(param(start), heights)
        # The menu shows the surface: the map root's grGroundParam scale times the
        # joint height plus the platform line's Y in its joint.
        scale = struct.unpack_from(">f", DAT, ROOTS["grGroundParam"])[0]
        verts, _, lines, line_count = struct.unpack_from(">4I", DAT, ROOTS["coll_data"])
        line_ys = set()
        for index in range(line_count):
            at = 32 + lines + index * 16
            start, end = struct.unpack_from(">hh", DAT, at)
            if struct.unpack_from(">H", DAT, at + 14)[0] & 0x100:
                for vertex in (start, end):
                    line_ys.add(struct.unpack_from(">f", DAT, 32 + verts + vertex * 8 + 4)[0])
        self.assertEqual((scale, line_ys), (0.75, {1.5}))
        self.assertEqual(labels[FIRST:], ["%.3f" % (scale * (h + 1.5)) for h in heights])
        self.assertEqual((labels[FIRST], labels[-1]), ("12.375", "27.375"))

    def test_a_held_platform_stays_put_and_the_game_keeps_its_collision(self):
        m = Machine()
        left, right = FIRST + 4, FIRST + 40
        frames = 3000
        for frame in range(frames):
            m.frame(left, right)
            for index, choice in ((LEFT, left), (RIGHT, right)):
                side = m.side(index)
                self.assertEqual((side["state"], side["timer"]), (WAIT, 0xFF))
                self.assertEqual((side["height"], side["target"], side["joint_y"]),
                                 (joint_height(choice),) * 3)
        # Between Lab frames, the game's think leaves it where it is.
        m.think()
        self.assertEqual((m.side(LEFT)["state"], m.side(LEFT)["timer"]), (WAIT, 0xFE))
        self.assertEqual(m.side(LEFT)["joint_y"], joint_height(left))
        self.assertEqual(m.info(2), 2 * (frames + 1))
        self.assertEqual((m.info(3), m.info(4)), (1, 0))

        # The water column is the size the game draws at that height.
        native = Machine()
        for index, choice in ((LEFT, left), (RIGHT, right)):
            native.set_side(index, state=MOVE, height=joint_height(choice), target=joint_height(choice))
        native.think()
        for index in (LEFT, RIGHT):
            for a, b in zip(m.side(index)["water_scale"], native.side(index)["water_scale"]):
                self.assertClose(a, b)
            self.assertEqual(m.side(index)["joint_y"], native.side(index)["joint_y"])

    def test_hidden_matches_the_games_own_sink(self):
        native = Machine()
        sunk = native.sink_natively(LEFT)
        self.assertEqual(native.info(0), 1)

        m = Machine()
        for _ in range(200):
            m.frame(HIDE, RANDOM)
        held = m.side(LEFT)
        self.assertEqual((held["state"], held["timer"]), (GONE, 0xFF))
        for key in ("height", "target", "joint_y", "joint_flags", "column_flags", "water_flags"):
            self.assertEqual(held[key], sunk[key], key)
        self.assertTrue(held["column_flags"] & held["water_flags"] & HIDDEN)
        for a, b in zip(held["water_scale"], sunk["water_scale"]):
            self.assertClose(a, b)
        self.assertEqual(m.info(0), 1)  # The animation stops once, not every frame.
        self.assertFalse(m.side(RIGHT)["column_flags"] & HIDDEN)

    def test_a_height_after_hidden_resurfaces_the_way_the_game_does(self):
        native = Machine()
        native.sink_natively(LEFT)
        native.set_side(LEFT, timer=-1)
        native.frame()
        self.assertEqual(native.side(LEFT)["state"], MOVE)
        game_anim = [native.info(i) for i in (1, 6, 7, 8, 9, 10)]

        m = Machine()
        m.sink_natively(LEFT)
        for _ in range(100):
            m.frame(FIRST + 20, RANDOM)
        side = m.side(LEFT)
        self.assertEqual((side["state"], side["joint_y"]), (WAIT, joint_height(FIRST + 20)))
        self.assertFalse((side["column_flags"] | side["water_flags"]) & HIDDEN)
        self.assertEqual([m.info(i) for i in (1, 6, 7, 8, 9, 10)], game_anim)
        self.assertEqual(game_anim, [1, LEFT, 0, 7, 0, 1])

    def test_random_picks_up_from_where_it_was_held(self):
        for choice in (FIRST + 30, HIDE):
            with self.subTest(choice=choice):
                m = Machine()
                for _ in range(10):
                    m.frame(choice, RANDOM)
                held = m.side(LEFT)
                for _ in range(256):
                    m.frame(RANDOM, RANDOM)
                self.assertEqual(m.side(LEFT)["height"], held["height"])
                m.frame(RANDOM, RANDOM)
                side = m.side(LEFT)
                if choice == HIDE:
                    # It resurfaces toward the resurface height, rising 0.15 a frame.
                    self.assertEqual((side["state"], side["target"]), (MOVE, param(0xC)))
                    self.assertFalse(side["column_flags"] & HIDDEN)
                else:
                    self.assertTrue(side["state"] == MOVE or side["timer"] >= param(0x38))
                previous = side["height"]
                for _ in range(4000):
                    m.frame(RANDOM, RANDOM)
                    height = m.side(LEFT)["height"]
                    self.assertLessEqual(abs(height - previous), param(0x28) + 1e-5)
                    previous = height
                self.assertEqual(m.info(4), 0)

    def test_random_leaves_the_game_alone(self):
        game, lab = Machine(), Machine()
        states = set()
        for _ in range(5000):
            game.frame()
            lab.frame(RANDOM, RANDOM)
            states.add(game.side(LEFT)["state"])
        for index in (LEFT, RIGHT):
            self.assertEqual(lab.side_bytes(index), game.side_bytes(index))
        self.assertIn(MOVE, states)  # The game really moved it meanwhile.

    def test_other_stages_lookalikes_and_bad_choices_are_left_alone(self):
        m = Machine(stage=32)
        before = [m.side_bytes(i) for i in range(4)]
        m.fn("TestFountainApply", FIRST, HIDE)
        self.assertEqual([m.side_bytes(i) for i in range(4)], before)

        m = Machine()
        before = [m.side_bytes(i) for i in range(4)]
        for left, right in ((-1, m.info(5)), (0x7FFF, -5)):
            m.fn("TestFountainApply", left & 0xFFFFFFFF, right & 0xFFFFFFFF)
        self.assertEqual([m.side_bytes(i) for i in range(4)], before)
        m.fn("TestFountainApply", FIRST, HIDE)
        self.assertEqual([m.side_bytes(i) for i in (FOUNTAIN, LOOKALIKE)], before[FOUNTAIN:])
        self.assertNotEqual(m.side_bytes(LEFT), before[LEFT])
        self.assertNotEqual(m.side_bytes(RIGHT), before[RIGHT])

        # A third line group isn't a side platform the Lab knows.
        m = Machine()
        data = m.fn("TestFountainSide", LEFT) + m.layout[0]
        m.cpu.mem_write(data + 0xC8, struct.pack(">h", 2))
        before = m.side_bytes(LEFT)
        m.fn("TestFountainApply", FIRST, FIRST)
        self.assertEqual(m.side_bytes(LEFT), before)


if __name__ == "__main__":
    unittest.main(verbosity=2)
