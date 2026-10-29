#ifndef TM_ZOOM_CAMERA_H
#define TM_ZOOM_CAMERA_H
/* "Camera" option for offstage events (Side-B Sweetspot, Edgeguard Training):
 * Normal, or Zoomed Out to see farther offstage. Each including event calls
 * ZoomCamera_Init from Event_Init, then ZoomCamera_Apply when its row changes,
 * and attaches ZoomCamera_Restore as a GObj's userdata cleanup. */
#include "events.h"

#define ZOOM_CAMERA_VALUES "Normal", "Zoomed Out"

// Stage camera settings are outside the practice savestate. Keep the original
// values so toggling modes never compounds the zoom or affects another event.
static struct {
    float left, right, top, bottom;
    float min_distance, max_distance;
} ZoomCamera_Normal;

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
    }
    Match_SetNormalCamera();
    Match_CorrectCamera();
}

/* The match has just built the stage's camera: keep it, then apply the mode. */
static void ZoomCamera_Init(int zoomed_out) {
    ZoomCamera_Normal.left = stc_stage->cambound_left;
    ZoomCamera_Normal.right = stc_stage->cambound_right;
    ZoomCamera_Normal.top = stc_stage->cambound_top;
    ZoomCamera_Normal.bottom = stc_stage->cambound_bottom;
    ZoomCamera_Normal.min_distance = stc_stage->x34;
    ZoomCamera_Normal.max_distance = stc_stage->x38;
    ZoomCamera_Apply(zoomed_out);
}

#endif
