#ifndef VEYR_ADPF_H
#define VEYR_ADPF_H

/*
 * Android Dynamic Performance Framework hints (Android 13+, API 33).
 *
 * Call once per rendered frame from the render thread. The first call creates a hint session
 * for the calling thread; every later call reports how long the previous frame took, so the OS
 * can raise CPU frequency early instead of reacting after a frame spike.
 *
 * Resolved with dlsym, so it is a harmless no-op on older Android.
 * VEYR_ADPF=0 disables it. VEYR_ADPF_TARGET_FPS (30..2000, default 600) sets the target frame time.
 */
void adpf_frame_tick(void);

#endif
