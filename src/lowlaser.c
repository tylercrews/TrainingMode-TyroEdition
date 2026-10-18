#include "events.h"

/* Squat startup, never SquatWait. A zero animation rate holds the compressed
 * pose while normal hurtbox collision and percent accumulation remain live. */
#define PUFF_CROUCH_FRAME 2.f
/* FD's main-stage edge. Ground positions scale from FD's original X values
 * to each stage's live edges, so FD keeps exactly 0/±35/±70 and the player ±50. */
#define FD_EDGE 85.5656967f
#define MOVE_SPECIAL_N 18 /* Native move ID copied to atk_kind by ActionStateChange. */
#define FIRST_SPECIAL_STATE (ASID_BARRELCANNONWAIT + 1) /* 341: character-specific states. */
enum { OPT_POSITION, OPT_HEIGHT, OPT_OSDS, OPT_SUPPRESS, OPT_EXIT, OPT_RESET_PERCENT };
enum {
    POS_CENTER, POS_FAR_LEFT, POS_CENTER_LEFT, POS_CENTER_RIGHT, POS_FAR_RIGHT,
    POS_ON_LEFT, POS_ON_TOP, POS_ON_RIGHT, POS_UNDER_LEFT, POS_UNDER_RIGHT, POS_COUNT
};
enum { HEIGHT_GROUNDED, HEIGHT_SHORT, HEIGHT_FULL, HEIGHT_SHORT_DOUBLE, HEIGHT_FULL_DOUBLE, HEIGHT_COUNT };
typedef struct LowLaserData {
    EventDesc *desc;
    int initialized, airborne, air_frame, laser_frame, fastfall_frame, fastfall, prev_state;
    unsigned restore_serial;
    Vec3 puff_position; /* Surface point; Puff is held puff_height above it. */
    float puff_height;
    int puff_line, puff_tracks_line;
    Text *text[2];
} LowLaserData;
typedef char low_laser_data_fits[(sizeof(LowLaserData) <= EVENT_DATASIZE) ? 1 : -1];

static void ChangePosition(GOBJ *menu, int value);
static void ChangeHeight(GOBJ *menu, int value);
static void ChangeOSDs(GOBJ *menu, int value);
static void ChangeSuppression(GOBJ *menu, int value);
static void Exit(GOBJ *menu);
static void ResetPercent(GOBJ *menu);
static const char *positions[POS_COUNT] = {
    "Center", "Far Left", "Center Left", "Center Right", "Far Right",
    "On Left Plat", "On Top Plat", "On Right Plat", "Under Left Plat", "Under Right Plat"};
static const char *positions_unavailable[POS_COUNT] = {
    "Center", "Far Left", "Center Left", "Center Right", "Far Right",
    "On Left Plat (N/A)", "On Top Plat (N/A)", "On Right Plat (N/A)", "Under Left Plat (N/A)", "Under Right Plat (N/A)"};
static const char *positions_display[POS_COUNT];
static int position_available[POS_COUNT];
static int previous_position;
static const float ground_x[] = {0, -70, -35, 35, 70};
static const char *heights[] = {"Grounded", "Short Hop", "Full Hop", "Short Hop + Double Jump", "Full Hop + Double Jump"};
static EventOption options[] = {
    {.kind = OPTKIND_STRING, .name = "Puff Position", .value_num = POS_COUNT, .values = positions_display,
     .OnChange = ChangePosition,
     .desc = {"Place Puff on the ground or a platform.", "Unavailable platforms are skipped.", "Moving Puff keeps its percent. This setting is saved."}},
    {.kind = OPTKIND_STRING, .name = "Puff Height", .value_num = countof(heights), .values = heights,
     .OnChange = ChangeHeight,
     .desc = {"Raise Puff above its position by one of", "Puff's jump apex heights.", "This setting is saved."}},
    {.kind = OPTKIND_TOGGLE, .name = "Event OSDs", .val = 1, .OnChange = ChangeOSDs,
     .desc = {"Show Laser Start and the regular Fastfall timing.", "Laser Start counts from the first airborne frame.", "Both results stay white. This setting is saved."}},
    {.kind = OPTKIND_TOGGLE, .name = "Suppress Global OSDs", .OnChange = ChangeSuppression,
     .desc = {"Hide global OSDs during this event only.", "Event OSDs keep their own On/Off setting.", "Defaults Off. This setting is saved."}},
    {.kind = OPTKIND_FUNC, .name = "Exit", .OnSelect = Exit,
     .desc = {"Return to the Event Selection Screen."}},
    {.kind = OPTKIND_FUNC, .name = "Reset %", .OnSelect = ResetPercent,
     .desc = {"Reset only Jigglypuff's percent to 0."}},
};
static EventMenu menu = {.name = "Low Laser Puff Practice", .option_num = countof(options), .options = options, .on_exit = Exit};
EventMenu *Event_Menu = &menu;

static LowLaserData *Data(void) { return event_vars->event_gobj->userdata; }

/* Live collision geometry, as in Techchase: current vertices follow Fountain's
 * moving platforms, and only enabled lines count (Stadium's base layout). */
static int IsPlatformLine(const CollLineDesc *line) {
    /* Native MapLine.lo_flags LINE_FLAG_PLATFORM is 0x0100. */
    return ((const u8 *)line)[0xE] & 1;
}
static int FloorLine(int i, Vec2 *a, Vec2 *b, int *platform) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts || i < 0 || i >= stage->line_num) return 0;
    CollLineDesc *line = lines[i].desc;
    if (!line || !line->is_floor || line->disabled || !GrColl_CheckIfLineEnabled(i) ||
        line->vert_prev < 0 || line->vert_next < 0 ||
        line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num) return 0;
    *a = verts[line->vert_prev].pos_curr;
    *b = verts[line->vert_next].pos_curr;
    if (a->X > b->X) { Vec2 swap = *a; *a = *b; *b = swap; }
    *platform = IsPlatformLine(line);
    return b->X - a->X >= 0.01f;
}
static int LineCount(void) { return *stc_colldata ? (*stc_colldata)->line_num : 0; }
static int LineY(int i, float x, float *y) {
    Vec2 a, b;
    int platform;
    if (!FloorLine(i, &a, &b, &platform) || x < a.X || x > b.X) return 0;
    *y = a.Y + (b.Y - a.Y) * (x - a.X) / (b.X - a.X);
    return 1;
}
static int FindSolidGround(float x, Vec3 *position, int *index) {
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || platform || !LineY(i, x, &y)) continue;
        if (!found || y > position->Y) { *position = (Vec3){x, y, 0}; *index = i; found = 1; }
    }
    return found;
}
static void GroundEdges(float *left, float *right) {
    *left = -FD_EDGE; *right = FD_EDGE;
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform) || platform) continue;
        if (!found || a.X < *left) *left = a.X;
        if (!found || b.X > *right) *right = b.X;
        found = 1;
    }
}
/* Solid ground at x, moving toward center if a non-legal stage has a gap. */
static void GroundPoint(float x, Vec3 *position, int *index) {
    float inward = x;
    for (int i = 0; i < 4; ++i, inward *= .5f)
        if (FindSolidGround(inward, position, index)) return;
    Vec3 slope;
    int kind;
    *position = (Vec3){x, 0, 0};
    *index = -1;
    GrColl_RaycastGround(position, index, &kind, &slope, -1, -1, -1, 0, x, 10, x, -20, 0);
}
static int LegalStage(void) {
    int stage = Stage_GetExternalID();
    return stage == GRKINDEXT_STORY || stage == GRKINDEXT_IZUMI || stage == GRKINDEXT_FD ||
        stage == GRKINDEXT_PSTAD || stage == GRKINDEXT_BATTLE || stage == GRKINDEXT_OLDPU;
}
/* side -1/0/1: left, top, right one-way platform. Puff stands at its center. */
static int FindPlatform(int side, Vec3 *position, int *index) {
    if (!LegalStage()) return 0;
    int found = 0;
    float best = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || !platform || b.X - a.X < 10) continue;
        float x = (a.X + b.X) * .5f;
        if (side < 0 ? x >= -10 : side > 0 ? x <= 10 : a.X > 0 || b.X < 0) continue;
        if (!LineY(i, x, &y)) continue;
        float score = side ? -fabs(x) : y;
        if (!found || score > best) { *position = (Vec3){x, y, 0}; *index = i; best = score; found = 1; }
    }
    return found;
}
static int ResolvePosition(int choice, Vec3 *position, int *index, int *tracks) {
    *tracks = 0;
    if (choice < POS_ON_LEFT) {
        float left, right, x = ground_x[choice];
        GroundEdges(&left, &right);
        GroundPoint(x * ((x < 0 ? -left : right) / FD_EDGE), position, index);
        return 1;
    }
    static const signed char sides[] = {-1, 0, 1, -1, 1};
    Vec3 platform;
    int platform_index;
    if (!FindPlatform(sides[choice - POS_ON_LEFT], &platform, &platform_index)) return 0;
    if (choice <= POS_ON_RIGHT) {
        *position = platform; *index = platform_index; *tracks = 1;
        return 1;
    }
    return FindSolidGround(platform.X, position, index) && position->Y < platform.Y - 4;
}
static void RefreshPositions(void) {
    for (int i = 0; i < POS_COUNT; ++i) {
        Vec3 position;
        int index, tracks;
        position_available[i] = ResolvePosition(i, &position, &index, &tracks);
        positions_display[i] = position_available[i] ? positions[i] : positions_unavailable[i];
    }
}

/* Apex rise for one jump: the native jump applies gravity before moving. */
static float RiseHeight(float velocity, float gravity) {
    float height = 0;
    for (int i = 0; i < 256; ++i) {
        velocity -= gravity;
        if (velocity <= 0) break;
        height += velocity;
    }
    return height;
}
static float PuffHeight(FighterData *ft, int option) {
    if (option <= HEIGHT_GROUNDED || option >= HEIGHT_COUNT) return 0;
    /* Puff's ISO values, used only if live attributes are unusable. */
    float gravity = .064f, hop = 1.05f, full = 1.6f, air = 1.65f;
    if (ft->attr.gravity > 0) {
        gravity = ft->attr.gravity;
        hop = ft->attr.hop_v_initial_velocity;
        full = ft->attr.jump_v_initial_velocity;
        air = full * ft->attr.air_jump_v_multiplier;
        /* Multi-jumpers take aerial jump velocity from their own table. */
        if (ft->kind == FTKIND_JIGGLYPUFF && ft->special_attributes)
            air = ((FtMultiJumpDesc *)ft->special_attributes)->jump_vel_y[0];
    }
    float first = RiseHeight(option == HEIGHT_SHORT || option == HEIGHT_SHORT_DOUBLE ? hop : full, gravity);
    return first + (option >= HEIGHT_SHORT_DOUBLE ? RiseHeight(air, gravity) : 0);
}

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
static void GroundAt(GOBJ *fighter, Vec3 ground, int line) {
    FighterData *ft = fighter->userdata;
    if (line >= 0) ft->coll_data.ground_index = line;
    ft->phys.pos = ground;
    ClearVelocity(ft);
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}
/* Current surface under Puff. A platform position follows its live line
 * (Fountain); if that line is gone, Puff uses the ground below until it returns. */
static Vec3 PuffSurface(LowLaserData *data, int *line) {
    Vec3 surface = data->puff_position, ground;
    *line = data->puff_line;
    if (data->puff_tracks_line && !LineY(data->puff_line, surface.X, &surface.Y) &&
        FindSolidGround(surface.X, &ground, line)) surface.Y = ground.Y;
    return surface;
}
static void HoldPuff(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    LowLaserData *data = Data();
    int line;
    Fighter_ZeroCPUInputs(ft);
    ft->cpu.ai = 15;
    ft->flags.no_reaction_always = 1;
    ft->flags.nudge_disable = 1;
    ft->grab.vuln = 0x1FF;
    ft->phys.pos = PuffSurface(data, &line);
    ft->phys.pos.Y += data->puff_height;
    if (data->puff_height <= 0 && line >= 0) ft->coll_data.ground_index = line;
    ClearVelocity(ft);
}
/* Airborne Puff is held in place: no landing or ledge checks. A no-op rather
 * than NULL, because not every native caller checks the collision callback. */
static void HoldPuffCollision(GOBJ *fighter) { }
static int PuffState(void) { return Data()->puff_height > 0 ? ASID_FALL : ASID_SQUAT; }
static void FreezePuff(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    LowLaserData *data = Data();
    data->puff_height = PuffHeight(ft, options[OPT_HEIGHT].val);
    HoldPuff(fighter);
    UpdatePosition(fighter);
    /* Apply the frozen pose without animation interpolation. */
    if (data->puff_height > 0) {
        Fighter_SetAirborne(ft);
        ActionStateChange(0, 0, -1, fighter, ASID_FALL, 0, 0);
    } else {
        EnvironmentCollision_WaitLanding(fighter);
        Fighter_SetGrounded(ft);
        ActionStateChange(PUFF_CROUCH_FRAME, 0, -1, fighter, ASID_SQUAT, 0, 0);
    }
    Fighter_SubactionFastForward(fighter);
    ft->cb.Anim = 0;
    ft->cb.IASA = 0;
    ft->cb.Phys = HoldPuff;
    if (data->puff_height > 0) ft->cb.Coll = HoldPuffCollision;
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
    RefreshPositions();
    int choice = options[OPT_POSITION].val;
    /* A saved platform missing on this stage falls back for this match only. */
    if (choice < 0 || choice >= POS_COUNT || !position_available[choice]) choice = POS_CENTER;
    options[OPT_POSITION].val = previous_position = choice;
    ResolvePosition(choice, &data->puff_position, &data->puff_line, &data->puff_tracks_line);
    float x = data->puff_position.X, left, right;
    int player_right = x < -1;
    GroundEdges(&left, &right);
    Vec3 ground;
    int line;
    GroundPoint(player_right ? 50 * (right / FD_EDGE) : -50 * (-left / FD_EDGE), &ground, &line);
    /* Rebirth also clears a previous KO; it does not reset Puff's percent. */
    Fighter_EnterSleep(human, 0);
    Fighter_EnterRebirth(human);
    GroundAt(human, ground, line);
    hmn->facing_direction = hmn->facing_direction_prev = player_right ? -1 : 1;
    Fighter_EnterWait(human);
    GroundAt(puff, data->puff_position, data->puff_line);
    cpu->facing_direction = cpu->facing_direction_prev = -hmn->facing_direction;
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
    data->prev_state = -1;
    ClearTiming(data);
    data->restore_serial = event_vars->get_restore_serial();
    for (int i = 0; i < POS_COUNT; ++i) positions_display[i] = positions[i];
    options[OPT_POSITION].val = options[OPT_POSITION].val_prev = TM_GetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_POSITION);
    options[OPT_HEIGHT].val = options[OPT_HEIGHT].val_prev = TM_GetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_HEIGHT);
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
static int IsBlaster(FighterData *ft) { return ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO; }
void Event_Think(GOBJ *gobj) {
    if (Pause_CheckStatus(1) == 2) return;
    LowLaserData *data = gobj->userdata;
    GOBJ *human = Fighter_GetGObj(0), *puff = Fighter_GetGObj(1);
    FighterData *hmn = human->userdata, *cpu = puff->userdata;
    if (!data->initialized) {
        data->initialized = 1;
        if (data->text[0] && !IsBlaster(hmn)) Text_SetText(data->text[0], 0, "Neutral-B Start");
        PlaceFighters();
    }
    if (data->restore_serial != event_vars->get_restore_serial()) {
        data->restore_serial = event_vars->get_restore_serial();
        ClearTiming(data);
        FreezePuff(puff);
    }
    if (hmn->flags.dead) { PlaceFighters(); return; }
    if (cpu->state_id != PuffState()) FreezePuff(puff);
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
     * Others: entering a character special state whose native move ID is
     * neutral-B. atk_kind persists after a move, so only a transition counts.
     * Grounded shots are not a low aerial-laser attempt. */
    int neutral_b = IsBlaster(hmn) ? hmn->state_id == 344 :
        hmn->state_id != data->prev_state && hmn->state_id >= FIRST_SPECIAL_STATE && hmn->atk_kind == MOVE_SPECIAL_N;
    if (airborne && neutral_b && data->laser_frame < 0)
        data->laser_frame = data->air_frame;
    if (airborne && hmn->flags.is_fastfall && !data->fastfall) {
        /* Reuse the exact native counter consumed by Display FF Timing.asm:
         * 1 means the first eligible fastfall frame, not time since takeoff. */
        data->fastfall_frame = hmn->TM.can_fastfall_frames;
    }
    data->airborne = airborne;
    data->fastfall = airborne && hmn->flags.is_fastfall;
    data->prev_state = hmn->state_id;
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
static void ChangePosition(GOBJ *menu, int value) {
    RefreshPositions();
    /* Keep moving in the selector's direction past missing platforms. */
    int direction = (value - previous_position + POS_COUNT) % POS_COUNT == 1 ? 1 : -1;
    for (int i = 0; i < POS_COUNT && !position_available[value]; ++i)
        value = (value + direction + POS_COUNT) % POS_COUNT;
    options[OPT_POSITION].val = previous_position = value;
    TM_SetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_POSITION, value);
    if (Data()->initialized) PlaceFighters();
}
static void ChangeHeight(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_LOW_LASER, TM_LOW_LASER_HEIGHT, value);
    if (Data()->initialized) PlaceFighters();
}
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
