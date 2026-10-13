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

// Everything below comes from libandroid.so at runtime: the NDK headers need API 33 at compile time.
typedef void *(*adpf_get_manager_fn)(void);
typedef void *(*adpf_create_session_fn)(void *manager, const int32_t *threadIds, size_t size,
                                         int64_t initialTargetWorkDurationNanos);
typedef int (*adpf_report_actual_fn)(void *session, int64_t actualDurationNanos);
typedef int (*adpf_update_target_fn)(void *session, int64_t targetDurationNanos);

typedef void *(*athermal_acquire_fn)(void);
typedef int (*athermal_get_status_fn)(void *manager);
typedef void (*athermal_status_cb)(void *data, int status);
typedef int (*athermal_register_fn)(void *manager, athermal_status_cb callback, void *data);

// Target frame time = 1 / fps. ADPF only boosts the CPU while frames take LONGER than the target,
// so for an uncapped 400+ fps goal the target must sit well below the real frame time (600 fps = 1.67 ms).
// A low target keeps the boost almost always on (more heat); lower VEYR_ADPF_TARGET_FPS to trade FPS for temperature.
#define ADPF_DEFAULT_TARGET_FPS 600
#define ADPF_MAX_TARGET_FPS 2000
#define ADPF_MAX_REPORTED_FRAME_NS 250000000LL // ignore pauses (loading screens, background)
#define ADPF_REPORT_WINDOW_NS 8000000LL        // at very high fps, batch ~8 ms of frames per report

static pthread_once_t g_loadOnce = PTHREAD_ONCE_INIT;
static int g_hintAvailable = 0;
static int g_thermalAdapt = 1;
static int g_baseFps = ADPF_DEFAULT_TARGET_FPS;
static int g_thermalStatus = -1; // written from a binder thread, read from the render thread (atomics)
static void *g_manager = NULL;
static adpf_create_session_fn p_createSession = NULL;
static adpf_report_actual_fn p_reportActual = NULL;
static adpf_update_target_fn p_updateTarget = NULL;

static __thread int t_tried = 0;
static __thread void *t_session = NULL;
static __thread int t_appliedStatus = -1;
static __thread int64_t t_lastFrameNs = 0;
static __thread int64_t t_windowNs = 0;
static __thread int t_windowFrames = 0;

static int env_disabled(const char *name) {
    const char *value = getenv(name);
    return value != NULL && value[0] == '0';
}

static void on_thermal_status(void *data, int status) {
    (void) data;
    __atomic_store_n(&g_thermalStatus, status, __ATOMIC_RELAXED);
    printf("veyr/thermal: status changed to %d\n", status);
}

static void thermal_load(void *lib) {
    athermal_acquire_fn acquire = (athermal_acquire_fn) dlsym(lib, "AThermal_acquireManager");
    athermal_get_status_fn getStatus = (athermal_get_status_fn) dlsym(lib, "AThermal_getCurrentThermalStatus");
    athermal_register_fn registerCallback =
            (athermal_register_fn) dlsym(lib, "AThermal_registerThermalStatusListener");
    if (acquire == NULL || getStatus == NULL || registerCallback == NULL) return; // needs Android 11+

    void *manager = acquire();
    if (manager == NULL) return;

    int status = getStatus(manager); // one-off call on the first frame, never again from the render thread
    __atomic_store_n(&g_thermalStatus, status, __ATOMIC_RELAXED);
    int result = registerCallback(manager, on_thermal_status, NULL);
    printf("veyr/thermal: watching thermal status (initial %d, listener %s)\n",
           status, result == 0 ? "ok" : "failed");
}

static void adpf_load(void) {
    void *lib = dlopen("libandroid.so", RTLD_NOW);
    if (lib == NULL) return;

    g_thermalAdapt = !env_disabled("VEYR_ADPF_THERMAL");
    thermal_load(lib); // also feeds the frame statistics, so it runs even when ADPF itself is off

    if (env_disabled("VEYR_ADPF")) {
        printf("veyr/adpf: disabled\n");
        return;
    }

    adpf_get_manager_fn getManager = (adpf_get_manager_fn) dlsym(lib, "APerformanceHint_getManager");
    p_createSession = (adpf_create_session_fn) dlsym(lib, "APerformanceHint_createSession");
    p_reportActual = (adpf_report_actual_fn) dlsym(lib, "APerformanceHint_reportActualWorkDuration");
    p_updateTarget = (adpf_update_target_fn) dlsym(lib, "APerformanceHint_updateTargetWorkDuration");
    if (getManager == NULL || p_createSession == NULL || p_reportActual == NULL) {
        printf("veyr/adpf: not available on this Android version (needs 13+)\n");
        return;
    }

    g_manager = getManager();
    if (g_manager == NULL) {
        printf("veyr/adpf: PerformanceHintManager unavailable\n");
        return;
    }

    const char *fpsEnv = getenv("VEYR_ADPF_TARGET_FPS");
    if (fpsEnv != NULL) {
        int parsed = atoi(fpsEnv);
        if (parsed >= 30 && parsed <= ADPF_MAX_TARGET_FPS) g_baseFps = parsed;
    }
    g_hintAvailable = 1;
    printf("veyr/adpf: ready, base target %d fps\n", g_baseFps);
}

// Eases the target only; it never raises it above the base value, so the boost is never increased by heat.
static int effective_fps(int status) {
    int fps = g_baseFps;
    if (!g_thermalAdapt || status < 0) return fps;
    if (status >= 3 && fps > 120) fps = 120;
    else if (status == 2 && fps > 240) fps = 240;
    return fps;
}

int adpf_thermal_status(void) {
    pthread_once(&g_loadOnce, adpf_load);
    return __atomic_load_n(&g_thermalStatus, __ATOMIC_RELAXED);
}

void adpf_frame_tick(void) {
    pthread_once(&g_loadOnce, adpf_load);
    if (!g_hintAvailable) return;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = (int64_t) ts.tv_sec * 1000000000LL + (int64_t) ts.tv_nsec;
    int status = __atomic_load_n(&g_thermalStatus, __ATOMIC_RELAXED);

    if (!t_tried) {
        t_tried = 1;
        int32_t tid = (int32_t) syscall(__NR_gettid);
        int fps = effective_fps(status);
        t_session = p_createSession(g_manager, &tid, 1, 1000000000LL / fps);
        t_appliedStatus = status;
        if (t_session != NULL) {
            printf("veyr/adpf: hint session created for render thread %d (target %d fps)\n", (int) tid, fps);
        } else {
            printf("veyr/adpf: could not create a hint session\n");
        }
    }

    if (t_session != NULL && p_updateTarget != NULL && status != t_appliedStatus) {
        t_appliedStatus = status;
        int fps = effective_fps(status);
        p_updateTarget(t_session, 1000000000LL / fps);
        printf("veyr/adpf: thermal status %d -> target %d fps\n", status, fps);
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
