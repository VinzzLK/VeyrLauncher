#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "adpf.h"

// Opaque handles: the NDK header needs API 33 at compile time, so everything is resolved at runtime.
typedef void *(*adpf_get_manager_fn)(void);
typedef void *(*adpf_create_session_fn)(void *manager, const int32_t *threadIds, size_t size,
                                         int64_t initialTargetWorkDurationNanos);
typedef int (*adpf_report_actual_fn)(void *session, int64_t actualDurationNanos);

// Target frame time = 1 / fps. ADPF only boosts the CPU while frames take LONGER than the target,
// so for an uncapped 400+ fps goal the target must sit well below the real frame time (600 fps = 1.67 ms).
// A low target keeps the boost almost always on (more heat); lower VEYR_ADPF_TARGET_FPS to trade FPS for temperature.
#define ADPF_DEFAULT_TARGET_FPS 600
#define ADPF_MAX_TARGET_FPS 2000
#define ADPF_MAX_REPORTED_FRAME_NS 250000000LL // ignore pauses (loading screens, background)
#define ADPF_REPORT_WINDOW_NS 8000000LL        // at very high fps, batch ~8 ms of frames per report

static pthread_once_t g_loadOnce = PTHREAD_ONCE_INIT;
static int g_available = 0;
static int64_t g_targetNs = 0;
static void *g_manager = NULL;
static adpf_create_session_fn p_createSession = NULL;
static adpf_report_actual_fn p_reportActual = NULL;

static __thread int t_tried = 0;
static __thread void *t_session = NULL;
static __thread int64_t t_lastFrameNs = 0;
static __thread int64_t t_windowNs = 0;
static __thread int t_windowFrames = 0;

static void adpf_load(void) {
    const char *env = getenv("VEYR_ADPF");
    if (env != NULL && env[0] == '0') {
        printf("veyr/adpf: disabled\n");
        return;
    }

    void *lib = dlopen("libandroid.so", RTLD_NOW);
    if (lib == NULL) return;

    adpf_get_manager_fn getManager = (adpf_get_manager_fn) dlsym(lib, "APerformanceHint_getManager");
    p_createSession = (adpf_create_session_fn) dlsym(lib, "APerformanceHint_createSession");
    p_reportActual = (adpf_report_actual_fn) dlsym(lib, "APerformanceHint_reportActualWorkDuration");
    if (getManager == NULL || p_createSession == NULL || p_reportActual == NULL) {
        printf("veyr/adpf: not available on this Android version (needs 13+)\n");
        return;
    }

    g_manager = getManager();
    if (g_manager == NULL) {
        printf("veyr/adpf: PerformanceHintManager unavailable\n");
        return;
    }

    int fps = ADPF_DEFAULT_TARGET_FPS;
    const char *fpsEnv = getenv("VEYR_ADPF_TARGET_FPS");
    if (fpsEnv != NULL) {
        int parsed = atoi(fpsEnv);
        if (parsed >= 30 && parsed <= ADPF_MAX_TARGET_FPS) fps = parsed;
    }
    g_targetNs = 1000000000LL / fps;
    g_available = 1;
    printf("veyr/adpf: ready, target %d fps (%lld ns)\n", fps, (long long) g_targetNs);
}

void adpf_frame_tick(void) {
    pthread_once(&g_loadOnce, adpf_load);
    if (!g_available) return;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = (int64_t) ts.tv_sec * 1000000000LL + (int64_t) ts.tv_nsec;

    if (!t_tried) {
        t_tried = 1;
        int32_t tid = (int32_t) syscall(__NR_gettid);
        t_session = p_createSession(g_manager, &tid, 1, g_targetNs);
        if (t_session != NULL) {
            printf("veyr/adpf: hint session created for render thread %d\n", (int) tid);
        } else {
            printf("veyr/adpf: could not create a hint session\n");
        }
    }

    if (t_session != NULL && t_lastFrameNs != 0) {
        int64_t frameNs = now - t_lastFrameNs;
        if (frameNs > 0 && frameNs < ADPF_MAX_REPORTED_FRAME_NS) {
            t_windowNs += frameNs;
            t_windowFrames++;
            if (t_windowNs >= ADPF_REPORT_WINDOW_NS) {
                // average frame time of the window; at 60 fps every frame is its own report
                p_reportActual(t_session, t_windowNs / t_windowFrames);
                t_windowNs = 0;
                t_windowFrames = 0;
            }
        } else {
            t_windowNs = 0;
            t_windowFrames = 0;
        }
    }
    t_lastFrameNs = now;
}
