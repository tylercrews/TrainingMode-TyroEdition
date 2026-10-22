"""Execute the event-local CPU protection palette through the real GX wrapper."""
import struct
import unittest

from test_settings import Machine


COLORS = [0,0xFF464670,0x4691FF70,0xFFE14170,0x4BE16470,0x00FFFF70,0xFF00FF70,0xB48CFF70]


class CPUInvincibilityOverlayTests(unittest.TestCase):
    def setUp(self):
        self.m=Machine();self.m.init();self.m.call("TestCueInit")
        self.option=0x80407000
        for slot in range(4):self.m.call("TestCueState",slot,14,0,40,100)
        self.m.call("TestCuePlayer",0,1,0)
        self.m.call("TestCuePlayer",1,3,1)
        self.palette(7)

    def palette(self,value,bind=True):
        self.m.cpu.mem_write(self.option,struct.pack(">h",value))
        if bind:self.m.call("ActionCues_SetCPUInvincibilityOverlay",self.option)

    def draw(self,slot):
        self.m.call("TestCueDraw",slot)
        return self.m.call("TestCueDrawColor",slot)

    def protect(self,slot,status=2):
        self.m.call("TestCueProtection",slot,status,0,0)

    def test_palette_is_bounded_and_uses_existing_translucent_protection_alpha(self):
        for choice,color in enumerate(COLORS):
            self.assertEqual(self.m.call("TMCue_CPUInvincibilityColor",choice),color)
        for choice in (8,255,0xFFFFFFFF):
            self.assertEqual(self.m.call("TMCue_CPUInvincibilityColor",choice),0)

    def test_global_off_local_palette_enables_only_protected_cpu_main_and_subfighter(self):
        m=self.m
        for slot in range(4):self.protect(slot)
        before=m.record()
        self.palette(6)
        self.assertEqual([self.draw(slot) for slot in range(4)], [0,0,COLORS[6],COLORS[6]])
        self.assertEqual(m.call("Settings_Get",11,7),0)
        self.assertEqual(m.record(),before)
        self.protect(2,0)
        self.assertEqual(self.draw(2),0)
        self.assertEqual(self.draw(3),COLORS[6])

    def test_local_off_overrides_global_on_and_detach_restores_global_cpu_lavender(self):
        m=self.m;m.call("Settings_Set",11,7,1)
        for slot in range(4):self.protect(slot)
        before=m.record()
        self.palette(0)
        self.assertEqual([self.draw(slot) for slot in range(4)],[COLORS[2],COLORS[2],0,0])
        m.call("ActionCues_SetCPUInvincibilityOverlay",0)
        self.assertEqual([self.draw(slot) for slot in range(4)],[COLORS[2],COLORS[2],COLORS[7],COLORS[7]])
        self.assertEqual(m.call("Settings_Get",11,7),1)
        self.assertEqual(m.record(),before)

    def test_every_palette_and_off_on_edits_apply_while_paused_without_a_tick(self):
        self.protect(2);self.palette(0)
        self.assertEqual(self.m.call("TestCueWrapped",2),1)
        self.assertEqual(self.draw(2),0)
        for choice in list(range(8))+[1,0,7,-1,8]:
            self.palette(choice,bind=False)
            self.assertEqual(self.draw(2),COLORS[choice] if 0<=choice<8 else 0)
        self.m.call("ActionCues_SetCPUInvincibilityOverlay",0)
        self.assertEqual(self.m.call("TestCueWrapped",2),0)

    def test_exact_shared_protection_covers_aggregate_full_body_yoshi_and_counter(self):
        m=self.m;self.palette(5)
        data=m.call("TestCueData",2);obj=m.call("TestCueObject",2)
        for script,game in ((1,0),(2,0),(0,1),(0,2)):
            m.call("TestCueProtection",2,script,game,0)
            self.assertEqual(m.call("ActionCues_IsProtected",obj),1)
            self.assertEqual(self.draw(2),COLORS[5])
        m.call("TestCueProtection",2,0,0,0)
        m.call("TestCueCapsules",2,15,2)
        self.assertEqual(self.draw(2),COLORS[5])
        m.call("TestCueCapsule",2,4,0)
        self.assertEqual(self.draw(2),0)
        m.call("TestCueCapsules",2,0,0)
        m.call("TestCueKind",2,14);m.call("TestCueProtection",2,0,0,12000)
        self.assertEqual(self.draw(2),COLORS[5])
        m.call("TestCueKind",2,1)
        self.assertEqual(self.draw(2),0)
        m.call("TestCueProtection",2,0,0,0)
        for kind in (18,26):
            m.call("TestCueKind",2,kind)
            for state in (369,371):
                m.call("TestCueState",2,state,10,60,100)
                for flags,color in ((0,0),(0x40,0),(0x80,0),(0xC0,COLORS[5])):
                    m.cpu.mem_write(data+0x221B,bytes([flags]))
                    self.assertEqual(self.draw(2),color)

    def test_paused_off_with_global_on_cannot_leave_stale_lavender_after_unpause(self):
        m=self.m;m.call("Settings_Set",11,7,1);self.protect(2)
        data=m.call("TestCueData",2)
        baseline=bytes(m.cpu.mem_read(data+0x408,384))
        self.palette(7);self.assertEqual(self.draw(2),COLORS[7])
        m.call("TestCueFrozen",2,1)
        self.palette(0,bind=False)
        self.assertEqual(self.draw(2),0)
        m.call("TestCueFrozen",2,0);m.call("TestCueTick",1)
        self.assertEqual(self.draw(2),0)
        self.assertEqual(bytes(m.cpu.mem_read(data+0x408,384)),baseline)
        m.call("Settings_Set",11,0,1);m.call("Settings_Set",11,8,1)
        self.palette(1,bind=False)
        self.assertEqual(self.draw(2),COLORS[1])  # Text overrides do not hide body cues.

    def test_draw_restores_all_native_colors_and_respects_timing_priority(self):
        m=self.m;self.palette(1);self.protect(2)
        data=m.call("TestCueData",2)
        m.cpu.mem_write(data+0x408,bytes((i*13+5)&255 for i in range(384)))
        m.call("TestCueLocal",2,0xAABBCCDD)
        before=bytes(m.cpu.mem_read(data,0x23EC))
        self.assertEqual(self.draw(2),COLORS[1])
        self.assertEqual(bytes(m.cpu.mem_read(data,0x23EC)),before)
        m.call("Settings_Set",11,4,1)
        m.call("TestCueState",2,42,3,5,100)
        m.call("TestCueTick",1)
        self.assertEqual(self.draw(2),0xFFF000DC)
        m.call("TestCueState",2,14,0,40,100);m.call("TestCueTick",2)
        self.assertEqual(self.draw(2),0x50FF5ADC)
        m.call("Settings_Set",11,4,0)
        self.assertEqual(self.draw(2),COLORS[1])

    def test_dropped_invincibility_retains_precedence_over_cpu_palette(self):
        m=self.m;self.palette(4);self.protect(2)
        tangent=self.option+4;m.cpu.mem_write(tangent,struct.pack(">h",1))
        m.call("ActionCues_SetTangibilityOverlay",tangent)
        self.assertEqual(self.draw(2),0)
        self.protect(2,0)
        self.assertEqual(self.draw(2),0xFF2828B4)
        self.protect(2)
        m.cpu.mem_write(tangent,bytes(2))
        self.assertEqual(self.draw(2),COLORS[4])

    def test_restore_rewind_death_and_respawn_keep_event_binding_scene_change_detaches_it(self):
        m=self.m;self.protect(2);self.palette(3)
        m.call("ActionCues_Clear")
        self.assertEqual(self.draw(2),COLORS[3])
        m.call("TestCueTick",10);m.call("TestCueTick",3)
        self.assertEqual(self.draw(2),COLORS[3])
        m.call("TestCueSpawn",2,2,1)
        self.assertEqual(self.draw(2),0)
        m.call("TestCueSpawn",2,3,0);m.call("TestCueTick",4)
        self.assertEqual(self.draw(2),COLORS[3])
        m.call("ActionCues_SceneChange")
        # The old event option may now be gone. Poison its address and start
        # a fresh match without registering any local CPU palette.
        m.cpu.mem_write(self.option,struct.pack(">h",1))
        m.call("TestCueInit")
        m.call("TestCuePlayer",1,3,1);m.call("TestCueState",2,14,0,40,100)
        self.protect(2)
        self.assertEqual(self.draw(2),0)
        m.call("Settings_Set",11,7,1);m.call("TestCueTick",5)
        self.assertEqual(self.draw(2),COLORS[7])

    def test_all_cpu_ports_and_subfighters_share_override_human_ports_keep_global_hues(self):
        m=self.m;m.call("Settings_Set",11,7,1);self.palette(2)
        for ply in range(6):m.call("TestCuePlayer",ply,ply%4,ply%2)
        for slot in range(12):
            m.call("TestCueState",slot,14,0,40,100);self.protect(slot)
        m.call("TestCueTick",1)
        hues=[COLORS[1],COLORS[2],COLORS[3],COLORS[4]]
        for slot in range(12):
            player=slot//2
            self.assertEqual(self.draw(slot),COLORS[2] if player%2 else hues[player%4])


if __name__=="__main__":unittest.main()
