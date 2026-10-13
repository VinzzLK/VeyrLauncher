#ifndef VEYR_ADPF_H
#define VEYR_ADPF_H

/*
 * Android Dynamic Performance Framework hints (Android 13+, API 33) plus a thermal watcher (API 30+).
 *
 * adpf_frame_tick(): call once per rendered frame from the render thread. The first call creates a hint
 * session for the calling thread; later calls report how long frames take so the OS can raise CPU frequency
 * before a frame spike. Everything is resolved with dlsym, so it is a harmless no-op on older Android.
 *
 * Thermal adaptation: the OS pushes thermal status changes to a listener (no polling, no binder call on the
 * render thread). At MODERATE the target eases to 240 fps and at SEVERE+ to 120 fps, which lets the OS lower
 * the boost before it throttles by force. It never caps the game's FPS.
 *
 * Env: VEYR_ADPF=0 disables the hint session, VEYR_ADPF_THERMAL=0 disables thermal easing,
 *      VEYR_ADPF_TARGET_FPS (30..2000, default 600) sets the base target frame time.
 */
void adpf_frame_tick(void);

/* Latest thermal status (0 none .. 6 shutdown) or -1 when unknown. */
int adpf_thermal_status(void);

#endif
