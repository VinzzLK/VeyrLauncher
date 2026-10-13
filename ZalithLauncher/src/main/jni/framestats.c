#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "adpf.h"
#include "framestats.h"

#define FS_BUCKET_NS 250000LL          // 0.25 ms per bucket
#define FS_BUCKETS 400                 // 0..100 ms, the last bucket also holds everything slower
#define FS_MAX_FRAME_NS 250000000LL    // pauses (loading screens, background) are not frames
#define FS_MIN_FRAMES_TO_REPORT 10

typedef struct {
    int64_t lastNs;
    int64_t windowStartNs;
    uint64_t frames;
    uint64_t sumNs;
    int64_t worstNs;
    uint32_t hist[FS_BUCKETS];
} fs_state;

static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static int g_enabled = 1;
static int64_t g_windowNs = 10000000000LL;
static __thread fs_state t_state;

static void fs_load(void) {
    const char *off = getenv("VEYR_FRAME_STATS");
    if (off != NULL && off[0] == '0') g_enabled = 0;
    const char *seconds = getenv("VEYR_FRAME_STATS_SECONDS");
    if (seconds != NULL) {
        int parsed = atoi(seconds);
        if (parsed >= 1 && parsed <= 600) g_windowNs = (int64_t) parsed * 1000000000LL;
    }
}

static int64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000000000LL + (int64_t) ts.tv_nsec;
}

static void fs_report(fs_state *s, int64_t now) {
    if (s->frames >= FS_MIN_FRAMES_TO_REPORT && s->sumNs > 0) {
        double avgFps = (double) s->frames * 1e9 / (double) s->sumNs;

        // 1% low: frame time of the slowest 1% of frames, read from the histogram.
        uint64_t slowest = (s->frames + 99) / 100;
        uint64_t seen = 0;
        int bucket = FS_BUCKETS - 1;
        for (; bucket > 0; bucket--) {
            seen += s->hist[bucket];
            if (seen >= slowest) break;
        }
        double lowMs = ((double) bucket + 0.5) * ((double) FS_BUCKET_NS / 1e6);
        printf("veyr/stats: %ds | avg %.0f fps | 1%% low %.0f fps | worst %.1f ms | thermal %d\n",
               (int) ((now - s->windowStartNs) / 1000000000LL), avgFps, 1000.0 / lowMs,
               (double) s->worstNs / 1e6, adpf_thermal_status());
    }
    memset(s->hist, 0, sizeof(s->hist));
    s->frames = 0;
    s->sumNs = 0;
    s->worstNs = 0;
    s->windowStartNs = now;
}

void framestats_tick(void) {
    pthread_once(&g_once, fs_load);
    if (!g_enabled) return;

    fs_state *s = &t_state;
    int64_t now = mono_ns();
    if (s->windowStartNs == 0) s->windowStartNs = now;

    if (s->lastNs != 0) {
        int64_t frameNs = now - s->lastNs;
        if (frameNs > 0 && frameNs < FS_MAX_FRAME_NS) {
            int64_t bucket = frameNs / FS_BUCKET_NS;
            if (bucket >= FS_BUCKETS) bucket = FS_BUCKETS - 1;
            s->hist[bucket]++;
            s->frames++;
            s->sumNs += (uint64_t) frameNs;
            if (frameNs > s->worstNs) s->worstNs = frameNs;
        }
    }
    s->lastNs = now;

    if (now - s->windowStartNs >= g_windowNs) fs_report(s, now);
}
