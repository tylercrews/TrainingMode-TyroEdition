#ifndef TM_GRAB_MASH_H
#define TM_GRAB_MASH_H

#include "events.h"

/* CPU Mashing: how fast a held CPU mashes. 2v1 Grab Punish's enemy and
 * Grabs-ercise's Hit mode CPU mash the same way, from the same saved value
 * (TM_SETTING_GRAB_PUNISH's TM_GP_MASH). The game takes each input in
 * ftCommon_GrabMash (Fighter_CheckGrabBreakout, 8007DC08): 6 frames off the
 * grab timer for a fresh press of A, B, X or Y, and 6 more when the stick moves
 * into a new direction (past half tilt, either axis, at most once a frame), on
 * top of the 1 a frame that CapturePulled and CaptureWait take (800DB8A4,
 * 800DB908). Each setting is a number of mash inputs a second, so the timer
 * drops 1 + inputs a second / 10 a frame:
 *              inputs/s  a frame
 *   No Mash         0       1   nothing but the frame
 *   Light          10       2   a button every 6 frames
 *   Medium         20       3   a button every 3 frames (Grab Punish's default)
 *   Heavy          30       4   a button and a new stick direction, each
 *                               every 4 frames
 *   Extreme        40       5   a button and a new stick direction, each
 *                               every 3 frames
 *   Inhuman        50       6   a button every 2 frames and a new stick
 *                               direction every 3
 * The rates come from the event author's own mash practice (T10): 20 a second
 * comes easily, 25 is about average, 35 is a really solid stretch, and 40 was
 * the peak, reached only after practicing for a while. So Extreme is about the
 * best a practiced human does, and Inhuman is just past it. The game would
 * count up to 120 a second (a new button and stick direction every frame,
 * which Grab Punish's Inhuman used to be), but no one mashes that fast. Before
 * T10 the fourth setting was Very Heavy at 60 a second; Extreme keeps its saved
 * value, and Inhuman keeps Inhuman's. The stick only goes left and right, and
 * no button is X or Y, so the grab always ends in the ground release
 * (CaptureCut), never the jump release (CaptureJump). */

// Saved by position: Extreme took Very Heavy's value and Inhuman kept its own.
enum grab_mash { MASH_NONE, MASH_LIGHT, MASH_MEDIUM, MASH_HEAVY, MASH_EXTREME, MASH_INHUMAN, MASH_COUNT };
static const char *GrabMash_Text[MASH_COUNT] = {
    "No Mash", "Light Mash", "Medium Mash", "Heavy Mash", "Extreme Mash", "Inhuman Mash",
};
// Frames taken off the grab timer a frame by each setting, on average:
// 1 + inputs a second / 10, for 0, 10, 20, 30, 40 and 50 inputs a second.
static const int GrabMash_Rate[MASH_COUNT] = { 1, 2, 3, 4, 5, 6 };
typedef char grab_mash_choices[(int)MASH_COUNT == TM_GP_MASH_COUNT ? 1 : -1];

/* A grab sets the held player's timer as it connects (800DA944): 30 + 1.6 x
 * percent + 8 x (9 - handicap) + 15 x (3 - placing), with handicap 9 when it's
 * off and placing 0 for first. Taking the placing and handicap terms back out
 * holds every grab as if the match were tied with handicap off, 75 + 1.6 x
 * percent frames without mashing, which is what each setting's breakout times
 * are worked out from. Once a hold, on its first frame. */
static inline void GrabMash_EvenHold(FighterData *held, int ply) {
    ftCommonData *common = *stc_ftcommon;
    held->grab.grab_timer += common->grab_placing_mult * Fighter_GetCurrentPlacing(ply)
        - common->grab_mash_per_handicap * (common->grab_max_handicap - Fighter_GetHandicap(ply));
}

/* The CPU's inputs this frame for a setting; frame counts from the grab. They
 * go on top of zeroed CPU inputs. */
static inline void GrabMash_Inputs(FighterData *cpu, int mash, int frame) {
    switch (mash) {
        case MASH_LIGHT:
            if (frame % 6 < 3) cpu->cpu.held = PAD_BUTTON_A;
            break;
        case MASH_MEDIUM:
            if (frame % 3 == 0) cpu->cpu.held = PAD_BUTTON_A;
            break;
        case MASH_HEAVY:
            if (frame % 4 < 2) cpu->cpu.held = frame / 4 % 2 ? PAD_BUTTON_B : PAD_BUTTON_A;
            cpu->cpu.lstickX = (frame + 2) / 4 % 2 ? 127 : -127;
            break;
        case MASH_EXTREME:
            // A on frames 0, 3, 6..., the stick flipping on 2, 5, 8...
            if (frame % 3 == 0) cpu->cpu.held = PAD_BUTTON_A;
            cpu->cpu.lstickX = (frame + 1) / 3 % 2 ? 127 : -127;
            break;
        case MASH_INHUMAN:
            // A on even frames, the stick flipping on 1, 4, 7...
            if (frame % 2 == 0) cpu->cpu.held = PAD_BUTTON_A;
            cpu->cpu.lstickX = (frame + 2) / 3 % 2 ? 127 : -127;
            break;
    }
}

#endif
