#define _GNU_SOURCE // we are GNU GPLv3

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bigcoreaffinity.h"

#define VEYR_MAX_CPUS 64
#define VEYR_BIG_PERCENT 70

static pthread_once_t g_detectOnce = PTHREAD_ONCE_INIT;
static cpu_set_t g_bigMask;
static int g_bigCount = 0;   // 0 = unknown / nothing worth pinning
static int g_totalCount = 0;

static long read_max_freq_khz(int cpu) {
    char path[96];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
    FILE *file = fopen(path, "re");
    if (file == NULL) return -1;
    long freq = -1;
    if (fscanf(file, "%ld", &freq) != 1) freq = -1;
    fclose(file);
    return freq;
}

static void detect_big_cores(void) {
    long freqs[VEYR_MAX_CPUS];
    long maxFreq = 0;
    int total = (int) sysconf(_SC_NPROCESSORS_CONF);
    if (total < 1) total = 1;
    if (total > VEYR_MAX_CPUS) total = VEYR_MAX_CPUS;

    CPU_ZERO(&g_bigMask);
    g_totalCount = total;
    g_bigCount = 0;

    for (int i = 0; i < total; i++) {
        freqs[i] = read_max_freq_khz(i);
        if (freqs[i] <= 0) {
            // Topology is not fully readable (offline core / SELinux): do not guess.
            printf("veyr/bigcore: cpu%d max freq unreadable, topology unknown\n", i);
            return;
        }
        if (freqs[i] > maxFreq) maxFreq = freqs[i];
    }

    int big = 0;
    for (int i = 0; i < total; i++) {
        if (freqs[i] * 100 >= maxFreq * VEYR_BIG_PERCENT) {
            CPU_SET(i, &g_bigMask);
            big++;
        }
    }
    g_bigCount = big;
    printf("veyr/bigcore: %d of %d cores are big (max %ld kHz)\n", big, total, maxFreq);
}

int veyr_big_core_count(void) {
    pthread_once(&g_detectOnce, detect_big_cores);
    return g_bigCount;
}

void veyr_pin_render_thread(void) {
    static __thread int attempted = 0;
    if (attempted) return;
    attempted = 1;

    const char *env = getenv("VEYR_PIN_RENDER_THREAD");
    if (env != NULL && env[0] == '0') {
        printf("veyr/bigcore: render thread pinning disabled\n");
        return;
    }

    int big = veyr_big_core_count();
    // Symmetric CPUs (all cores "big") gain nothing from pinning.
    if (big <= 0 || big >= g_totalCount) {
        printf("veyr/bigcore: nothing to pin (big=%d total=%d)\n", big, g_totalCount);
        return;
    }

    // pid 0 = the calling thread only.
    if (sched_setaffinity(0, sizeof(cpu_set_t), &g_bigMask) != 0) {
        printf("veyr/bigcore: sched_setaffinity failed: %s\n", strerror(errno));
    } else {
        printf("veyr/bigcore: render thread pinned to %d big cores\n", big);
    }
}
