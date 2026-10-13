#ifndef VEYR_BIGCOREAFFINITY_H
#define VEYR_BIGCOREAFFINITY_H

/*
 * Big-core helpers (Veyr).
 *
 * "Big" cores are the cores whose cpuinfo_max_freq is at least 70% of the fastest core.
 * On a Snapdragon 870 that is 1x prime (3.2 GHz) + 3x gold (2.4 GHz); the 4x A55 are excluded.
 */

/* Number of big cores, or 0 when the topology cannot be read. Safe to call from any thread. */
int veyr_big_core_count(void);

/*
 * Pins ONLY the calling thread to the big-core mask and gives it a slightly higher priority (nice -4),
 * once per thread.
 * Never call this from the thread that later spawns the JVM: child threads inherit the mask.
 * Set VEYR_PIN_RENDER_THREAD=0 to disable.
 */
void veyr_pin_render_thread(void);

#endif
