/* Production event, with fixtures only at native engine boundaries. */
#include "../src/upbsweetspot.c"
enum { HMN, MARTH, PLAYERS };
static EventVars vars;
static GOBJ event, fighters[PLAYERS];
static FighterData ft[PLAYERS];
static JOBJ joints[PLAYERS];
static CmSubject subjects[PLAYERS];
static unsigned preferences[TM_UPB_PREF_COUNT] = {1, MARTH_TIPPER, 0};
static int swings, freezes, loads;
static float marth_rate = 1;

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
int sprintf(char *out, const char *fmt, ...) { *out = 0; return 0; }
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; }
static const TMSettingsAPI api = {Read, Write, 0};
static int Save(void *state, int flags) { return 1; }
static int Load(void *state, int flags) {
    ++loads;
    vars.game_timer = 1;
    /* A savestate puts Marth back in Wait at full speed. */
    ft[MARTH].state_id = ASID_WAIT;
    ft[MARTH].state.frame = 0;
    marth_rate = 1;
    return 1;
}
GOBJ *Fighter_GetGObj(int player) { return player < PLAYERS ? &fighters[player] : 0; }
GOBJ *Fighter_GetSubcharGObj(int player, int ms) { return 0; }
int Stage_GetExternalID(void) { return GRKINDEXT_FD; }
void Stage_GetLeftOfLineCoordinates(int line, Vec3 *pos) { *pos = (Vec3){-85, 0, 0}; }
void Stage_GetRightOfLineCoordinates(int line, Vec3 *pos) { *pos = (Vec3){85, 0, 0}; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *unk1, int unk2, int unk3, int unk4, void *cb,
    float x, float y, float to_x, float to_y, float unk5) { *pos = (Vec3){x, 0, 0}; *line = 1; return 1; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
void Fighter_UpdateCameraBox(GOBJ *f) { }
void Match_CorrectCamera(void) { }
void Fighter_EnterSleep(GOBJ *f, int ms) { }
void Fighter_EnterRebirth(GOBJ *f) { }
void Fighter_EnterRebirthWait(GOBJ *f) {
    FighterData *data = f->userdata;
    data->state_id = ASID_REBIRTHWAIT;
    data->phys.air_state = 1;
}
void Fighter_UpdateRebirthPlatformPos(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) { ((FighterData *)f->userdata)->state_id = ASID_WAIT; }
void FSmash_GetASForAnalogAngle(GOBJ *f) {
    FighterData *data = f->userdata;
    ++swings;
    data->state_id = ASID_ATTACKS4S;
    data->state.frame = 0;
}
void FrameSpeedChange(GOBJ *f, float speed) {
    if (speed == 0) ++freezes;
    marth_rate = speed;
}
void Fighter_ZeroCPUInputs(FighterData *f) { }
int SFX_Play(int id) { return 0; }
float sqrtf(float value) {
    float root = value > 1 ? value : 1;
    for (int i = 0; i < 24; ++i) root = (root + value / root) / 2;
    return root;
}
float HSD_Randf(void) { return .5f; }
void Fighter_SetHUDDamage(int player, u16 percent) { }

/* What the engine does to Marth between the event's thinks. */
static void Engine(void) {
    FighterData *marth = &ft[MARTH];
    if (marth->state_id < ASID_ATTACKS4HI || marth->state_id > ASID_ATTACKS4LW) return;
    marth->state.frame += marth_rate;
    ftHit *tipper = &marth->hitbox[MARTH_TIPPER_HITBOX];
    tipper->active = marth->state.frame >= MARTH_TIPPER_FRAME;
    tipper->pos.X = marth->phys.pos.X + marth->facing_direction * 30;
}
void TestUpBLoaded(void) { }
int TestUpBInit(int marth_on) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event;
    vars.settings = &api;
    vars.Savestate_Save_v1 = (void *)Save;
    vars.Savestate_Load_v1 = (void *)Load;
    for (int i = 0; i < PLAYERS; ++i) {
        fighters[i].userdata = &ft[i];
        fighters[i].hsd_object = &joints[i];
        ft[i].ply = i;
        ft[i].camera_subject = &subjects[i];
    }
    Options_Main[OPT_MARTH].val = marth_on ? MARTH_TIPPER : MARTH_OFF;
    GetLedgePositions(ledge_positions);
    vars.game_timer = 1;
    return 0;
}
void TestUpBThink(int frames) {
    while (frames--) {
        Engine();
        Event_Think(&event);
        ++vars.game_timer;
    }
}
void TestUpBFakeGrab(void) { ft[HMN].state_id = ASID_CLIFFCATCH; ft[HMN].phys.air_state = 1; }
void TestUpBStart(int state) {
    static float attributes[64];
    ft[HMN].special_attributes = attributes;
    ft[HMN].state_id = state;
    ft[HMN].kind = FTKIND_FOX;
    ft[HMN].phys.air_state = 1;
}
void TestUpBDown(int buttons) { ft[HMN].input.down = buttons; }
int TestUpBInfo(int key) {
    switch (key) {
    case 0: return swings;
    case 1: return freezes;
    case 2: return ft[MARTH].state_id >= ASID_ATTACKS4HI && ft[MARTH].state_id <= ASID_ATTACKS4LW;
    case 3: return marth_rate * 100;
    case 4: return ft[HMN].state_id == ASID_REBIRTHWAIT;
    case 5: return ft[MARTH].hitbox[MARTH_TIPPER_HITBOX].active;
    case 6: return ft[MARTH].hitbox[MARTH_TIPPER_HITBOX].pos.X * 10;
    case 7: return loads;
    }
    return -1;
}
