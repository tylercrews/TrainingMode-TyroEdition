#include "events.h"

/* Squat startup, never SquatWait. A zero animation rate holds the compressed
 * pose while normal hurtbox collision and percent accumulation remain live. */
#define PUFF_CROUCH_FRAME 2.f
enum { OPT_POSITION, OPT_OSDS, OPT_SUPPRESS, OPT_EXIT, OPT_RESET_PERCENT };
typedef struct LowLaserData {
    EventDesc *desc;
    int initialized, airborne, air_frame, laser_frame, fastfall_frame, fastfall;
    unsigned restore_serial;
    Vec3 puff_position;
    Text *text[2];
} LowLaserData;
typedef char low_laser_data_fits[(sizeof(LowLaserData) <= EVENT_DATASIZE) ? 1 : -1];

static void ChangePosition(GOBJ *menu, int value);
static void ChangeOSDs(GOBJ *menu, int value);
static void ChangeSuppression(GOBJ *menu, int value);
static void Exit(GOBJ *menu);
static void ResetPercent(GOBJ *menu);
static const char *positions[] = {"Center", "Far Left", "Center Left", "Center Right", "Far Right"};
static const float position_x[] = {0, -70, -35, 35, 70};
static EventOption options[] = {
    {.kind = OPTKIND_STRING, .name = "Puff Position", .value_num = countof(positions), .values = positions,
     .OnChange = ChangePosition,
     .desc = {"Place Puff across Final Destination.", "The player starts on the opposite side.", "Moving Puff keeps its accumulated percent."}},
    {.kind = OPTKIND_TOGGLE, .name = "Event OSDs", .val = 1, .OnChange = ChangeOSDs,
     .desc = {"Show Laser Start and the regular Fastfall timing.", "Laser Start counts from the first airborne frame.", "Both results stay white. This setting is saved."}},
    {.kind = OPTKIND_TOGGLE, .name = "Suppress Global OSDs", .OnChange = ChangeSuppression,
     .desc = {"Hide global OSDs during this event only.", "Event OSDs keep their own On/Off setting.", "Defaults Off. This setting is saved."}},
    {.kind = OPTKIND_FUNC, .name = "Exit", .OnSelect = Exit,
     .desc = {"Return to the Event Selection Screen."}},
    {.kind = OPTKIND_FUNC, .name = "Reset %", .OnSelect = ResetPercent,
     .desc = {"Reset only Jigglypuff's percent to 0."}},
};
static EventMenu menu = {.name = "Low Laser Puff Practice", .option_num = countof(options), .options = options};
EventMenu *Event_Menu = &menu;

static LowLaserData *Data(void) { return event_vars->event_gobj->userdata; }
static void ClearVelocity(FighterData *ft) {
    ft->phys.self_vel = ft->phys.kb_vel = ft->phys.self_vel_ground = (Vec3){0};
    ft->phys.atk_shield_kb_vel = (Vec3){0};
    ft->phys.nudge_vel = (Vec2){0};
}
static void UpdatePosition(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    Vec3 pos = ft->phys.pos;
    ft->phys.pos_prev = pos;
    ft->coll_data.topN_Curr = ft->coll_data.topN_CurrCorrect = pos;
    ft->coll_data.topN_Prev = ft->coll_data.topN_Proj = pos;
    ft->coll_data.coll_test = *stc_colltest;
    JOBJ *joint = fighter->hsd_object;
    joint->trans = pos;
    JOBJ_SetMtxDirtySub(joint);
    Fighter_SetPosition(ft->ply, ft->flags.ms, &pos);
}
static void GroundAt(GOBJ *fighter, float x) {
    FighterData *ft = fighter->userdata;
    Vec3 ground = {x, 0, 0}, slope;
    int line, kind;
    if (GrColl_RaycastGround(&ground, &line, &kind, &slope, -1, -1, -1, 0,
            x, 10, x, -20, 0)) ft->coll_data.ground_index = line;
    ft->phys.pos = ground;
    ClearVelocity(ft);
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}
static void HoldPuff(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    Fighter_ZeroCPUInputs(ft);
    ft->cpu.ai = 15;
    ft->flags.no_reaction_always = 1;
    ft->flags.nudge_disable = 1;
    ft->grab.vuln = 0x1FF;
    ft->phys.pos = Data()->puff_position;
    ClearVelocity(ft);
}
static void FreezePuff(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    /* Apply the frozen pose without animation interpolation. */
    ActionStateChange(PUFF_CROUCH_FRAME, 0, -1, fighter, ASID_SQUAT, 0, 0);
    Fighter_SubactionFastForward(fighter);
    ft->cb.Anim = 0;
    ft->cb.IASA = 0;
    ft->cb.Phys = HoldPuff;
    HoldPuff(fighter);
    UpdatePosition(fighter);
    JOBJ_SetupMtxSub(fighter->hsd_object);
}
static void ClearTiming(LowLaserData *data) {
    data->airborne = data->air_frame = data->fastfall = 0;
    data->laser_frame = data->fastfall_frame = -1;
}
static void PlaceFighters(void) {
    LowLaserData *data = Data();
    GOBJ *human = Fighter_GetGObj(0), *puff = Fighter_GetGObj(1);
    FighterData *hmn = human->userdata, *cpu = puff->userdata;
    float x = position_x[options[OPT_POSITION].val];
    /* Rebirth also clears a previous KO; it does not reset Puff's percent. */
    Fighter_EnterSleep(human, 0);
    Fighter_EnterRebirth(human);
    GroundAt(human, x < 0 ? 50 : -50);
    hmn->facing_direction = hmn->facing_direction_prev = x < 0 ? -1 : 1;
    Fighter_EnterWait(human);
    GroundAt(puff, x);
    cpu->facing_direction = cpu->facing_direction_prev = -hmn->facing_direction;
    data->puff_position = cpu->phys.pos;
    FreezePuff(puff);
    Fighter_UpdateCameraBox(human);
    Fighter_UpdateCameraBox(puff);
    Match_CorrectCamera();
    ClearTiming(data);
}

void Event_Init(GOBJ *gobj) {
    LowLaserData *data = gobj->userdata;
    EventDesc *desc = data->desc;
    memset(data, 0, sizeof(*data));
    data->desc = desc;
    ClearTiming(data);
    data->restore_serial = event_vars->get_restore_serial();
    options[OPT_OSDS].val = options[OPT_OSDS].val_prev = TM_GetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_OSDS);
    options[OPT_SUPPRESS].val = options[OPT_SUPPRESS].val_prev = TM_GetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_SUPPRESS_GLOBAL);
    event_vars->set_osd_suppression(&options[OPT_SUPPRESS].val);
    /* Same centered HUD canvas, size and height as Under Fire's timing pair. */
    int canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    GXColor white = {255, 255, 255, 255};
    for (int i = 0; i < 2; ++i) {
        Text *text = data->text[i] = Text_CreateText(2, canvas);
        if (!text) continue;
        text->kerning = text->align = 1;
        text->trans = (Vec3){i ? 385 : 255, 100, 0};
        text->viewport_scale.X = text->viewport_scale.Y = 1;
        text->scale.X = text->scale.Y = .45f;
        Text_AddSubtext(text, 0, 0, i ? "Fastfall" : "Laser Start");
        Text_AddSubtext(text, 0, 22, "-");
        Text_SetColor(text, 0, &white);
        Text_SetColor(text, 1, &white);
    }
}
void Event_Think(GOBJ *gobj) {
    if (Pause_CheckStatus(1) == 2) return;
    LowLaserData *data = gobj->userdata;
    if (!data->initialized) {
        data->initialized = 1;
        PlaceFighters();
    }
    GOBJ *human = Fighter_GetGObj(0), *puff = Fighter_GetGObj(1);
    FighterData *hmn = human->userdata, *cpu = puff->userdata;
    if (data->restore_serial != event_vars->get_restore_serial()) {
        data->restore_serial = event_vars->get_restore_serial();
        ClearTiming(data);
        FreezePuff(puff);
    }
    if (hmn->flags.dead) { PlaceFighters(); return; }
    if (cpu->state_id != ASID_SQUAT) FreezePuff(puff);
    /* HoldPuff runs in the native physics pass, before collision and model
     * updates. Do not rewrite/dirty its joints here after those passes: this
     * callback runs at priority 16, and stops entirely while the menu is open. */
    if (hmn->flags.hitlag || hmn->flags.freeze) return;
    int airborne = hmn->phys.air_state != 0;
    if (airborne && !data->airborne) {
        data->air_frame = 0;
        data->laser_frame = data->fastfall_frame = -1;
    }
    if (airborne && data->air_frame < 32767) ++data->air_frame;
    /* Fox and Falco share SpecialAirNStart (344), Loop (345), End (346).
     * Grounded shots are not a low aerial-laser attempt. */
    if (airborne && hmn->state_id == 344 && data->laser_frame < 0)
        data->laser_frame = data->air_frame;
    if (airborne && hmn->flags.is_fastfall && !data->fastfall) {
        /* Reuse the exact native counter consumed by Display FF Timing.asm:
         * 1 means the first eligible fastfall frame, not time since takeoff. */
        data->fastfall_frame = hmn->TM.can_fastfall_frames;
    }
    data->airborne = airborne;
    data->fastfall = airborne && hmn->flags.is_fastfall;
}
void Event_Update(void) {
    LowLaserData *data = Data();
    int paused = ((MenuData *)event_vars->menu_gobj->userdata)->mode == MenuMode_Paused;
    int frames[] = {data->laser_frame, data->fastfall_frame};
    for (int i = 0; i < 2; ++i) {
        Text *text = data->text[i];
        if (!text) continue;
        text->hidden = paused || !options[OPT_OSDS].val;
        if (frames[i] < 0) Text_SetText(text, 1, "-");
        else Text_SetText(text, 1, "%df", frames[i]);
    }
}
static void ChangePosition(GOBJ *menu, int value) { if (Data()->initialized) PlaceFighters(); }
static void ChangeOSDs(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_OSDS, value); }
static void ChangeSuppression(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_SUPPRESS_GLOBAL, value); }
static void ResetPercent(GOBJ *menu) {
    FighterData *puff = Fighter_GetGObj(1)->userdata;
    puff->dmg.percent = puff->dmg.percent_temp = 0;
    Fighter_SetHUDDamage(puff->ply, 0);
}
static void Exit(GOBJ *menu) {
    event_vars->set_osd_suppression(0);
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}
