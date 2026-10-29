#ifndef TM_ZOOM_CAMERA_H
#define TM_ZOOM_CAMERA_H
/* "Camera" option for offstage events (Side-B Sweetspot, Edgeguard Training):
 * Normal, or Zoomed Out to see farther offstage. Each including event calls
 * ZoomCamera_Init from Event_Init, then ZoomCamera_Apply when its row changes,
 * and attaches ZoomCamera_Restore as a GObj's userdata cleanup. */
#include "events.h"

#define ZOOM_CAMERA_VALUES "Normal", "Zoomed Out"
/* How far below the bottom blast zone the camera frames, so its line and the
 * start of the Blast Zone Overlay's glow show under the fighters. */
#define ZOOM_CAMERA_BLAST_MARGIN 20.f

// Stage camera settings are outside the practice savestate. Keep the original
// values so toggling modes never compounds the zoom or affects another event.
static struct {
    float left, right, top, bottom;
    float min_distance, max_distance;
} ZoomCamera_Normal;

/* With show_blast_bottom, a camera subject on the bottom blast zone, focused
 * only while Zoomed Out. The camera only frames down to the lowest fighter, so
 * without it the blast zone stays off-screen until someone is about to die. */
static CmSubject *ZoomCamera_Bottom;

static void ZoomCamera_Restore(void *unused) {
    stc_stage->cambound_left = ZoomCamera_Normal.left;
    stc_stage->cambound_right = ZoomCamera_Normal.right;
    stc_stage->cambound_top = ZoomCamera_Normal.top;
    stc_stage->cambound_bottom = ZoomCamera_Normal.bottom;
    stc_stage->x34 = ZoomCamera_Normal.min_distance;
    stc_stage->x38 = ZoomCamera_Normal.max_distance;
}

static void ZoomCamera_Apply(int zoomed_out) {
    ZoomCamera_Restore(0);
    if (zoomed_out) {
        // Retain normal tracking of both fighters, with more recovery space on
        // either side. Widen the tracking limits too: distance alone still
        // clamps an offstage fighter to the stage's original camera bounds.
        stc_stage->cambound_left -= 80;
        stc_stage->cambound_right += 80;
        stc_stage->cambound_top += 40;
        stc_stage->cambound_bottom -= 40;
        stc_stage->x34 *= 1.5f;
        stc_stage->x38 *= 1.5f;
        // The limits and blast zones share the stage's camera offset, so this
        // lets the blast zone subject's box through.
        float below_blast = stc_stage->blastzoneBottom - ZOOM_CAMERA_BLAST_MARGIN;
        if (ZoomCamera_Bottom && stc_stage->cambound_bottom > below_blast)
            stc_stage->cambound_bottom = below_blast;
    }
    if (ZoomCamera_Bottom)
        ZoomCamera_Bottom->kind = zoomed_out ? 0 : 1; // 1 = not focused
    Match_SetNormalCamera();
    Match_CorrectCamera();
}

/* Keep the blast zone subject under the fighters, at x, so it only ever
 * stretches the camera down, never sideways. Call every frame. */
static void ZoomCamera_Follow(float x) {
    if (!ZoomCamera_Bottom)
        return;
    ZoomCamera_Bottom->cam_pos.X = x;
    ZoomCamera_Bottom->cam_pos.Y = Stage_GetBlastzoneBottom();
}

/* The match has just built the stage's camera: keep it, then apply the mode. */
static void ZoomCamera_Init(int zoomed_out, int show_blast_bottom) {
    ZoomCamera_Normal.left = stc_stage->cambound_left;
    ZoomCamera_Normal.right = stc_stage->cambound_right;
    ZoomCamera_Normal.top = stc_stage->cambound_top;
    ZoomCamera_Normal.bottom = stc_stage->cambound_bottom;
    ZoomCamera_Normal.min_distance = stc_stage->x34;
    ZoomCamera_Normal.max_distance = stc_stage->x38;
    // A retried event may keep the last match's pointer; that subject is gone.
    ZoomCamera_Bottom = 0;
    if (show_blast_bottom) {
        CmSubject *cam = CameraSubject_Alloc();
        cam->boundleft_proj = cam->boundleft_curr = 0;
        cam->boundright_proj = cam->boundright_curr = 0;
        cam->boundtop_proj = cam->boundtop_curr = 0;
        cam->boundbottom_proj = cam->boundbottom_curr = -ZOOM_CAMERA_BLAST_MARGIN;
        ZoomCamera_Bottom = cam;
        ZoomCamera_Follow(0);
    }
    ZoomCamera_Apply(zoomed_out);
}

#endif
