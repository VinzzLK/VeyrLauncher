#ifndef VEYR_FRAMESTATS_H
#define VEYR_FRAMESTATS_H

/*
 * Frame statistics for A/B testing. Call once per frame from the render thread.
 * Every 10 seconds it prints one line to the launcher log, for example:
 *   veyr/stats: 10s | avg 412 fps | 1% low 231 fps | worst 18.4 ms | thermal 0
 * Uses a fixed histogram (no allocation, no sorting), so it costs a few nanoseconds per frame.
 * Env: VEYR_FRAME_STATS=0 disables it, VEYR_FRAME_STATS_SECONDS (1..600, default 10) changes the window.
 */
void framestats_tick(void);

#endif
