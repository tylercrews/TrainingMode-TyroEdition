/* Production Passing Practice event, with a small engine at the native
 * boundaries: the teammate's jumpsquat, jump, drift and aerial, a scripted
 * hitbox, knockback that decays like the game's, landing on Final
 * Destination's floor, and hitlag. The procs run in the game's order around
 * the event's think: hitlag (0) and animation (1) before it, then input (3)
 * and physics (4), then hitbox spots and collision. */
#include "../src/passing.c"
#include <stdarg.h>

enum { HMN, MATE, ENEMY, PLAYERS };
static EventVars vars;
static GOBJ event, fighters[PLAYERS];
static FighterData ft[PLAYERS];
static JOBJ joints[PLAYERS];
static Playerblock players[6];
static CollDataStage test_stage;
static CollLine test_line;
static CollLineDesc test_descriptor;
static CollVert test_vertices[2];
static ftCommonData common;
static unsigned preferences[TM_PASS_PREF_COUNT] = {
    TM_PASS_DEFAULT_AERIAL, TM_PASS_DEFAULT_PERCENT, 0, TM_PASS_DEFAULT_DISTANCE, 0,
};
static int loads, saves, sfx, hud_percent[PLAYERS], randi;
static Text hud;
static char row_text[ROW_COUNT][48];
static GXColor counter_color;

/* The teammate's aerial: one hitbox, forward and up from its feet. */
static struct aerial_script {
    int first, last, length, lag; // Move frames: the first is the press frame.
    float x, y, r, damage;
} script = {4, 15, 30, 9, -9, 6, 5, 12};
static int anim[PLAYERS], kneebend_short, jump_first;
static int mate_prev_held, lcancel_timer = 255;
static float mate_prev_cx, mate_prev_cy, mate_prev_lx, mate_prev_ly;
static int hit_victim; // The enemy has been hit by this aerial.
static int landing_lag, lcancels;
static int mate_hits, player_hits;
static float decay = .051f;

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
/* About as cheap as the game's: a bit-level guess, then three Newton steps. */
float sqrtf(float value) {
    if (value <= 0) return 0;
    union { float f; u32 i; } guess = { value };
    guess.i = 0x1FBD1DF5 + (guess.i >> 1);
    float x = guess.f;
    for (int i = 0; i < 3; i++) x = .5f * (x + value / x);
    return x;
}
static char *Number(char *out, int value) {
    char digits[12];
    int count = 0;
    if (value < 0) { *out++ = '-'; value = -value; }
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value);
    while (count) *out++ = digits[--count];
    return out;
}
/* The event's formats: %d, %s and %%. */
static void Format(char *out, const char *fmt, va_list args) {
    for (; *fmt; ++fmt) {
        if (*fmt != '%') { *out++ = *fmt; continue; }
        ++fmt;
        if (*fmt == 'd') out = Number(out, va_arg(args, int));
        else if (*fmt == 's') { const char *s = va_arg(args, const char *); while (*s) *out++ = *s++; }
        else if (*fmt == '%') *out++ = '%';
    }
    *out = 0;
}
static u32 Read(unsigned field, unsigned index) { return field == TM_SETTING_PASSING ? preferences[index] : 0; }
static void Write(unsigned field, unsigned index, u32 value) { if (field == TM_SETTING_PASSING) preferences[index] = value; }
static const TMSettingsAPI api = {Read, Write, 0};
static int Save(void *state, int flags) { ++saves; return 1; }
/* The real load rewinds game_timer and every fighter. */
static int Load(void *state, int flags) {
    ++loads;
    vars.game_timer = 1;
    for (int i = 0; i < PLAYERS; ++i) {
        ft[i].dmg.percent = ft[i].dmg.percent_temp = 0;
        ft[i].phys.self_vel = ft[i].phys.kb_vel = (Vec3){0};
        ft[i].flags.freeze = 0;
        ft[i].dmg.hitlag_frames = 0;
        ft[i].state_id = ASID_WAIT;
        ft[i].phys.air_state = 0;
        ft[i].cb.Phys = 0;
        ft[i].cb.Coll = 0;
        ft[i].hitbox[0].active = 0;
    }
    return 1;
}
static void Panel(const char **label, const char **info, int count) { }

GOBJ *Fighter_GetGObj(int player) { return player < PLAYERS ? &fighters[player] : 0; }
GOBJ *Fighter_GetSubcharGObj(int player, int ms) { return 0; }
Playerblock *Fighter_GetPlayerblock(int player) { return &players[player]; }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int gx_link, int gx_pri) { }
/* The event's s_link 15 proc runs after the probe's collision. */
static void (*late_proc)(GOBJ *);
static GOBJ late_gobj;
GOBJ *GObj_Create(int entity_class, int p_link, int p_priority) { return &late_gobj; }
GOBJProc *GObj_AddProc(GOBJ *gobj, void *cb, int s_link) { if (s_link == 15) late_proc = cb; return 0; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
void Fighter_SetAirborne(FighterData *f) { f->phys.air_state = 1; }
int GrColl_CheckIfLineEnabled(int line) { return 1; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *slope, int a, int b, int c, void *d,
    float x1, float y1, float x2, float y2, float e) { return 0; }
int Stage_GetExternalID(void) { return GRKINDEXT_FD; }
void Fighter_SetAllHurtboxState(GOBJ *f, int kind) { }
void Fighter_UpdateCameraBox(GOBJ *f) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) { ((FighterData *)f->userdata)->state_id = ASID_WAIT; }
void Fighter_EnterSleep(GOBJ *f, int ms) { ((FighterData *)f->userdata)->state_id = ASID_SLEEP; }
void ActionStateChange(float frame, float rate, float blend, GOBJ *f, int state, int flags, GOBJ *alt) {
    ((FighterData *)f->userdata)->state_id = state;
}
void Match_CorrectCamera(void) { }
void Fighter_SetHUDDamage(int player, u16 percent) { hud_percent[player] = percent; }
void Fighter_ZeroCPUInputs(FighterData *f) {
    f->cpu.held = 0;
    f->cpu.lstickX = f->cpu.lstickY = f->cpu.cstickX = f->cpu.cstickY = 0;
}
int Pause_CheckStatus(int kind) { return 0; }
int SFX_Play(int id) { sfx = id; return 0; }
int HSD_Randi(int max) { return randi % max; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { }
int Text_CreateCanvas(int sis, GOBJ *cam, int a, int b, int c, int d, int e, int f) { return 0; }
Text *Text_CreateText(int sis, int canvas) { return &hud; }
int Text_AddSubtext(Text *text, float x, float y, char *string, ...) { static int count; return count++ % (ROW_COUNT * ROW_SUBTEXTS); }
void Text_SetScale(Text *text, int subtext, float x, float y) { }
void Text_SetColor(Text *text, int subtext, GXColor *color) {
    if (subtext == ROW_COUNTER * ROW_SUBTEXTS + ROW_FOREGROUND) counter_color = *color;
}
void Text_SetText(Text *text, int subtext, const char *fmt, ...) {
    if (subtext % ROW_SUBTEXTS != ROW_FOREGROUND) return;
    va_list args;
    va_start(args, fmt);
    Format(row_text[subtext / ROW_SUBTEXTS], fmt, args);
    va_end(args);
}

// The engine ----------------------------------------------------------------

static float Stick(s8 value) { return value > 0 ? value / 127.f : value / 128.f; }

/* Melee's knockback for a hit of `damage` at the enemy's percent, with
 * growth 100 and base 10, at 45 degrees (361 in the air). */
static void Launch(int attacker, float damage) {
    FighterData *e = &ft[ENEMY], *a = &ft[attacker];
    float p = e->dmg.percent + damage;
    float kb = ((p / 10 + p * damage / 20) * (200 / (e->attr.weight + 100)) * 1.4f + 18) + 10;
    float speed = kb * .03f * .70710678f;
    float away = e->phys.pos.X > a->phys.pos.X ? 1 : -1;
    e->phys.kb_vel = (Vec3){away * speed, speed, 0};
    e->phys.self_vel = (Vec3){0};
    e->dmg.percent = p;
    e->dmg.source_ply = attacker;
    e->state_id = ASID_DAMAGEFLYN;
    e->phys.air_state = 1;
    e->cb.Phys = 0;
    e->cb.Coll = 0;
    e->flags.freeze = a->flags.freeze = 1;
    e->dmg.hitlag_frames = a->dmg.hitlag_frames = 5;
}

static void EnterFall(FighterData *f) { f->state_id = ASID_FALL; anim[f - ft] = 0; }
static void Land(FighterData *f, int lag) {
    f->flags.is_fastfall = 0;
    f->phys.pos.Y = 0;
    f->phys.self_vel.Y = 0;
    f->phys.air_state = 0;
    f->state_id = lag ? ASID_LANDINGAIRN : ASID_WAIT;
    anim[f - ft] = 0;
    f->hitbox[0].active = 0;
}

/* Procs 0 and 1: hitlag runs down, then animations and scripts. */
static void BeforeThink(void) {
    for (int i = 0; i < PLAYERS; ++i) {
        FighterData *f = &ft[i];
        if (f->dmg.hitlag_frames > 0) {
            f->dmg.hitlag_frames -= 1;
            if (f->dmg.hitlag_frames <= 0) { f->dmg.hitlag_frames = 0; f->flags.freeze = 0; }
        }
    }
    FighterData *m = &ft[MATE];
    if (m->flags.freeze) return;
    int s = m->state_id;
    if (s == ASID_KNEEBEND) {
        if (++anim[MATE] >= (int)m->attr.jump_startup_time) {
            m->state_id = ASID_JUMPF;
            m->phys.air_state = 1;
            m->phys.self_vel.Y = kneebend_short ? m->attr.hop_v_initial_velocity : m->attr.jump_v_initial_velocity;
            float vx = m->phys.self_vel.X * m->attr.ground_to_air_jump_momentum_multiplier
                + mate_prev_lx * m->attr.jump_h_initial_velocity;
            m->phys.self_vel.X = Clamp(vx, -m->attr.jump_h_max_velocity, m->attr.jump_h_max_velocity);
            jump_first = 1;
            anim[MATE] = 0;
        }
    } else if (s >= ASID_ATTACKAIRN && s <= ASID_ATTACKAIRLW) {
        int move = ++anim[MATE] + 1;
        if (move == script.first) { m->hitbox[0].active = 1; hit_victim = 0; }
        if (move > script.last) m->hitbox[0].active = 0;
        if (move > script.length) { m->hitbox[0].active = 0; EnterFall(m); }
    } else if (s == ASID_LANDINGAIRN || s == ASID_TURN) {
        if (++anim[MATE] >= (s == ASID_TURN ? 3 : landing_lag)) { m->state_id = ASID_WAIT; anim[MATE] = 0; }
    }
}

/* Proc 3, after the event: the teammate's CPU inputs. */
static void EngineInput(void) {
    FighterData *m = &ft[MATE];
    int held = m->cpu.held, pressed = held & ~mate_prev_held;
    if (pressed & (PAD_TRIGGER_L | PAD_TRIGGER_R)) lcancel_timer = 0;
    else if (lcancel_timer < 255 && !m->flags.freeze) lcancel_timer++;
    float lx = Stick(m->cpu.lstickX), cx = Stick(m->cpu.cstickX), cy = Stick(m->cpu.cstickY);
    int s = m->state_id;
    if (!m->flags.freeze) {
        if (s >= ASID_WAIT && s <= ASID_RUNBRAKE && s != ASID_TURN) {
            if (pressed & PAD_BUTTON_X) {
                m->state_id = ASID_KNEEBEND;
                anim[MATE] = 0;
                kneebend_short = 0;
            } else if (fabs(lx) >= .8f && fabs(mate_prev_lx) < .3f) {
                m->facing_direction = Sign(lx);
                m->state_id = ASID_DASH;
                m->phys.self_vel.X = Sign(lx) * m->attr.dash_initial_velocity;
            } else if (fabs(lx) > .25f) {
                if (Sign(lx) != m->facing_direction) {
                    m->facing_direction = Sign(lx);
                    m->state_id = ASID_TURN;
                    anim[MATE] = 0;
                    m->phys.self_vel.X = 0;
                } else if (s != ASID_DASH) {
                    m->state_id = ASID_WALKMIDDLE;
                    m->phys.self_vel.X = lx * m->attr.walk_maximum_velocity;
                }
            } else {
                m->state_id = ASID_WAIT;
                m->phys.self_vel.X = 0;
            }
        } else if (s == ASID_KNEEBEND) {
            if (!(held & PAD_BUTTON_X)) kneebend_short = 1;
        } else if (s >= ASID_JUMPF && s <= ASID_FALLAERIALB) {
            int aerial = -1;
            bool fresh_c = (fabs(mate_prev_cx) < .3f && fabs(cx) >= .3f) || (fabs(mate_prev_cy) < .3f && fabs(cy) >= .3f);
            if (pressed & PAD_BUTTON_A) aerial = fabs(lx) < .28f ? 0 : lx * m->facing_direction > 0 ? 1 : 2;
            else if (fresh_c) aerial = cy > .5f ? 3 : cy < -.5f ? 4 : cx * m->facing_direction >= 0 ? 1 : 2;
            if (aerial >= 0) {
                m->state_id = ASID_ATTACKAIRN + aerial;
                anim[MATE] = 0;
                if (script.first == 1) { m->hitbox[0].active = 1; hit_victim = 0; }
            }
        }
    }
    mate_prev_held = held;
    mate_prev_lx = lx;
    mate_prev_ly = Stick(m->cpu.lstickY);
    mate_prev_cx = cx;
    mate_prev_cy = cy;
}

static float DriftOf(FighterData *f, float vx, float s) { return DriftStep(f, vx, s); }

/* Proc 4: physics, then landing. */
static void Physics(void) {
    FighterData *m = &ft[MATE];
    if (!m->flags.freeze) {
        if (m->phys.air_state) {
            if (jump_first) jump_first = 0;
            else {
                m->phys.self_vel.X = DriftOf(m, m->phys.self_vel.X, mate_prev_lx);
                if (!m->flags.is_fastfall && m->phys.self_vel.Y < 0 && mate_prev_ly <= -.66f) m->flags.is_fastfall = 1;
                m->phys.self_vel.Y = m->flags.is_fastfall ? -m->attr.fastfall_velocity : FallStep(m, m->phys.self_vel.Y);
            }
            float y = m->phys.pos.Y;
            m->phys.pos.X += m->phys.self_vel.X;
            m->phys.pos.Y += m->phys.self_vel.Y;
            if (m->phys.pos.Y <= 0 && y > 0) {
                int s = m->state_id;
                Land(m, s >= ASID_ATTACKAIRN && s <= ASID_ATTACKAIRLW);
                landing_lag = lcancel_timer < 7 ? script.lag / 2 : script.lag;
                lcancels += lcancel_timer < 7;
            }
        } else {
            if (m->state_id == ASID_KNEEBEND || m->state_id == ASID_WAIT) m->phys.self_vel.X *= .9f;
            m->phys.pos.X += m->phys.self_vel.X;
        }
    }
    FighterData *e = &ft[ENEMY];
    if (e->cb.Phys) {
        e->cb.Phys(&fighters[ENEMY]);
    } else if (!e->flags.freeze && e->phys.air_state) {
        e->phys.self_vel.Y = FallStep(e, e->phys.self_vel.Y);
        e->phys.self_vel.X = DriftOf(e, e->phys.self_vel.X, 0);
        float kx = e->phys.kb_vel.X, ky = e->phys.kb_vel.Y, k = sqrtf(kx * kx + ky * ky);
        if (k < decay) e->phys.kb_vel = (Vec3){0};
        else { e->phys.kb_vel.X -= decay * kx / k; e->phys.kb_vel.Y -= decay * ky / k; }
        float y = e->phys.pos.Y;
        e->phys.pos.X += e->phys.self_vel.X + e->phys.kb_vel.X;
        e->phys.pos.Y += e->phys.self_vel.Y + e->phys.kb_vel.Y;
        if (e->phys.pos.Y <= 0 && y > 0) {
            e->phys.pos.Y = 0;
            e->phys.air_state = 0;
            e->state_id = ASID_DOWNBOUNDU;
            e->phys.kb_vel = e->phys.self_vel = (Vec3){0};
        }
    }
}

/* After physics: the hitbox's spot, the enemy's hurtbox, then hits. */
static void Collide(void) {
    FighterData *m = &ft[MATE], *e = &ft[ENEMY];
    ftHit *hit = &m->hitbox[0];
    if (hit->active) {
        hit->pos_prev = hit->pos;
        hit->pos = (Vec3){m->phys.pos.X + m->facing_direction * script.x, m->phys.pos.Y + script.y, 0};
        if (hit->active == 1) hit->pos_prev = hit->pos;
        hit->active = hit->active == 1 ? 2 : 3;
        hit->size = script.r / m->scale.Y;
    }
    e->hurt_num = 1;
    e->hurtbox[0].hurt1_pos = (Vec3){e->phys.pos.X, e->phys.pos.Y + 3, 0};
    e->hurtbox[0].hurt2_pos = (Vec3){e->phys.pos.X, e->phys.pos.Y + 13, 0};
    if (hit->active >= 2 && !hit_victim && !e->flags.freeze) {
        // The enemy's body against the hitbox's sweep this frame.
        Vec2 c = {e->phys.pos.X, e->phys.pos.Y + 8};
        Vec2 a = {hit->pos_prev.X, hit->pos_prev.Y}, b = {hit->pos.X, hit->pos.Y};
        float dx = b.X - a.X, dy = b.Y - a.Y, len = dx * dx + dy * dy, t = 0;
        if (len > 0) t = Clamp(((c.X - a.X) * dx + (c.Y - a.Y) * dy) / len, 0, 1);
        float px = a.X + t * dx - c.X, py = a.Y + t * dy - c.Y;
        if (px * px + py * py < (script.r + 4) * (script.r + 4)) {
            hit_victim = 1;
            mate_hits++;
            Launch(MATE, script.damage);
        }
    }
}

// Tests -----------------------------------------------------------------------

/* Attributes from the ISO: Fox, Marth, Peach, Jigglypuff, Bowser, Pikachu. */
static const float fastfall[] = {3.4f, 2.5f, 2.f, 1.6f, 2.4f, 2.7f};
static const float attributes[][17] = {
    // startup, jump h, jump v, momentum, jump h max, hop v, gravity, terminal, drift mult,
    // drift base, drift max, friction, air max, weight, scale, dash, walk
    {3, .72f, 3.68f, .83f, 1.7f, 2.1f, .23f, 2.8f, .06f, .02f, .83f, .02f, 3, 75, .96f, 1.9f, 1.6f},
    {4, 1, 2.4f, .8f, 1.2f, 1.5f, .085f, 2.2f, .03f, .02f, .9f, .005f, 3, 87, 1.15f, 1.5f, 1.6f},
    {5, .7f, 2.2f, .7f, 1.1f, 1.6f, .08f, 1.5f, .06f, .01f, 1.1f, .005f, 3, 90, 1.15f, 1.2f, .85f},
    {5, .7f, 1.6f, 1, 1.35f, 1.05f, .064f, 1.3f, .09f, .19f, 1.35f, .05f, 3, 60, .94f, 1.4f, .7f},
    {8, 1, 2.8f, .9f, 1, 1.6f, .13f, 1.9f, .03f, .02f, .8f, .01f, 3, 117, .69f, 1, .65f},
    {3, .8f, 2.6f, .8f, 1.8f, 1.7f, .11f, 1.9f, .03f, .02f, .85f, .01f, 3, 80, .9f, 1.8f, 1.24f},
};
static void SetAttributes(FighterData *f, int who) {
    const float *a = attributes[who];
    f->attr.jump_startup_time = a[0];
    f->attr.jump_h_initial_velocity = a[1];
    f->attr.jump_v_initial_velocity = a[2];
    f->attr.ground_to_air_jump_momentum_multiplier = a[3];
    f->attr.jump_h_max_velocity = a[4];
    f->attr.hop_v_initial_velocity = a[5];
    f->attr.gravity = a[6];
    f->attr.terminal_velocity = a[7];
    f->attr.aerial_drift_stick_mult = a[8];
    f->attr.aerial_drift_base = a[9];
    f->attr.aerial_drift_max = a[10];
    f->attr.aerial_friction = a[11];
    f->attr.horizontal_air_mobility_constant = a[12];
    f->attr.weight = a[13];
    f->scale = (Vec3){a[14], a[14], a[14]};
    f->attr.dash_initial_velocity = a[15];
    f->attr.walk_maximum_velocity = a[16];
    f->attr.fastfall_velocity = fastfall[who];
    f->kind = who;
}

int TestPassInit(int mate_kind, int enemy_kind) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event;
    vars.settings = &api;
    vars.Savestate_Save_v1 = (void *)Save;
    vars.Savestate_Load_v1 = (void *)Load;
    vars.HUD_DrawInfoPanel = Panel;
    static MenuData menu_data;
    static GOBJ menu_gobj;
    menu_gobj.userdata = &menu_data;
    vars.menu_gobj = &menu_gobj;
    common.kb_frameDecay = decay;
    *stc_ftcommon = &common;
    for (int i = 0; i < PLAYERS; ++i) {
        fighters[i].userdata = &ft[i];
        fighters[i].hsd_object = &joints[i];
        ft[i].ply = i;
        ft[i].pad_index = i;
        ft[i].state.rate = 1;
        ft[i].facing_direction = 1;
        players[i].state = 1;
        players[i].p_kind = i == HMN ? PKIND_HMN : PKIND_CPU;
        players[i].team = i == ENEMY ? 1 : 0;
        SetAttributes(&ft[i], 0);
    }
    SetAttributes(&ft[MATE], mate_kind);
    SetAttributes(&ft[ENEMY], enemy_kind);
    test_stage.line_num = 1;
    test_stage.vert_num = 2;
    test_line.desc = &test_descriptor;
    test_descriptor.vert_prev = 0;
    test_descriptor.vert_next = 1;
    test_descriptor.is_floor = 1;
    test_vertices[0].pos_curr = (Vec2){-85.5657f, 0};
    test_vertices[1].pos_curr = (Vec2){85.5657f, 0};
    *stc_colldata = &test_stage;
    *stc_collline = &test_line;
    *stc_collvert = test_vertices;
    Event_Init(&event);
    return 0;
}
void TestPassScript(int first, int last, int x10, int y10, int r10) {
    script.first = first;
    script.last = last;
    script.x = x10 / 10.f;
    script.y = y10 / 10.f;
    script.r = r10 / 10.f;
}
void TestPassThink(int frames) {
    while (frames--) {
        BeforeThink();
        ++vars.game_timer;
        Event_Think(&event);
        EngineInput();
        Physics();
        Collide();
        if (late_proc) late_proc(&late_gobj);
        Event_Update();
    }
}
/* Your hit: the enemy launched with this velocity (hundredths a frame), as
 * after a collision at the end of last frame. */
void TestPassYouHit(int vx100, int vy100, int damage) {
    FighterData *e = &ft[ENEMY];
    e->phys.kb_vel = (Vec3){vx100 / 100.f, vy100 / 100.f, 0};
    e->phys.self_vel = (Vec3){0};
    e->dmg.percent += damage;
    e->dmg.source_ply = HMN;
    e->state_id = ASID_DAMAGEFLYN;
    e->phys.air_state = 1;
    e->cb.Phys = 0;
    e->flags.freeze = 1;
    e->dmg.hitlag_frames = 4;
    player_hits++;
}
/* One think, for measuring its cost against the emulator's budget. */
void TestPassThinkOnly(void) { Event_Think(&event); }
void TestPassPad(int down) { PadGetEngine(HMN)->down = down; PadGetEngine(HMN)->held = down; }
void TestPassChoose(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
void TestPassRandom(int value) { randi = value; }
int TestPassInfo(int key) {
    FighterData *e = &ft[ENEMY], *m = &ft[MATE];
    switch (key) {
    case 0: return phase;
    case 1: return volley.hits;
    case 2: return volley.last;
    case 3: return records[MODE_FROZEN].best;
    case 4: return records[MODE_FROZEN].previous;
    case 5: return records[MODE_RISING].best;
    case 6: return records[MODE_RISING].previous;
    case 7: return e->phys.pos.X * 10;
    case 8: return e->phys.pos.Y * 10;
    case 9: return e->phys.kb_vel.X * 100;
    case 10: return e->dmg.percent;
    case 11: return hud_percent[ENEMY];
    case 12: return m->phys.pos.X * 10;
    case 13: return m->phys.pos.Y * 10;
    case 14: return m->state_id;
    case 15: return mate_hits;
    case 16: return loads;
    case 17: return layout.toward;
    case 18: return layout.facing;
    case 19: return ft[HMN].phys.pos.X * 10;
    case 20: return measured.first;
    case 21: return measured.serve_known;
    case 22: return float_spot.X * 10;
    case 23: return float_spot.Y * 10;
    case 24: return jumps.liftoff;
    case 25: return Floating(e);
    case 26: return preferences[key - 26 + 0];
    case 27: return preferences[1];
    case 28: return preferences[2];
    case 29: return preferences[3];
    case 30: return preferences[4];
    case 31: return counter_color.r << 16 | counter_color.g << 8 | counter_color.b;
    case 32: return sfx;
    case 33: return m->facing_direction;
    case 34: return e->phys.air_state;
    case 35: return frame;
    case 36: return m->hitbox[0].active;
    case 37: return (int)(m->hitbox[0].pos.X * 10);
    case 38: return (int)(m->hitbox[0].pos.Y * 10);
    case 39: return e->facing_direction;
    case 40: return lcancels;
    }
    return -1;
}
char *TestPassText(int row) { return row_text[row]; }
/* The planner on the current state, for diagnosing a miss. */
static Plan debug_plan, debug_nearest;
static int debug_category;
void TestPassPlanNow(void) {
    FighterData *m = &ft[MATE], *e = &ft[ENEMY];
    GatherFloors();
    PredictEnemy(e, &path);
    debug_category = Category(m);
    debug_nearest = (Plan) { 0 };
    debug_plan = (Plan) { 0 };
    if (debug_category == CAT_GROUND) debug_plan = PlanFromGround(m, &path, &debug_nearest);
    else if (debug_category == CAT_AIR) debug_plan = PlanInAir(m, &path, control.lifted_at == frame ? 1 : 0);
}
int TestPassDebug(int key) {
    switch (key) {
    case 0: return path.frames;
    case 1: return path.lands;
    case 2: return debug_plan.valid;
    case 3: return debug_plan.wait;
    case 4: return debug_plan.full;
    case 5: return debug_plan.index;
    case 6: return debug_plan.at;
    case 7: return debug_plan.x * 10;
    case 8: return debug_nearest.valid;
    case 9: return debug_nearest.stick * 100;
    case 10: return debug_category;
    case 11: return debug_nearest.x * 10;
    case 12: return debug_nearest.at;
    case 13: return path.body[path.frames].X * 10;
    case 14: return measured.last;
    case 15: return control.aerial_at >= 0 ? control.aerial_index : -1;
    }
    return -1;
}
