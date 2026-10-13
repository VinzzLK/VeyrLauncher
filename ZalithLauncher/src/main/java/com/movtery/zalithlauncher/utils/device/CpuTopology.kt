package com.movtery.zalithlauncher.utils.device

import java.io.File

/**
 * Reads the CPU cluster layout from sysfs (Veyr).
 *
 * A core counts as "big" when its cpuinfo_max_freq is at least 70% of the fastest core.
 * On a Snapdragon 870 this gives 4 big cores (1 prime + 3 gold); the 4 Cortex-A55 are excluded.
 */
object CpuTopology {
    private const val BIG_CORE_PERCENT = 70
    private val CPU_DIR_REGEX = Regex("cpu\\d+")

    /** Number of big cores, or 0 when the topology cannot be read. */
    val bigCoreCount: Int by lazy { detectBigCoreCount() }

    /**
     * Value for -XX:ActiveProcessorCount.
     * Falls back to every core when the topology is unknown, symmetric, or has fewer than 2 big cores.
     */
    fun recommendedJvmProcessorCount(): Int {
        val total = Runtime.getRuntime().availableProcessors()
        val big = bigCoreCount
        return if (big in 2 until total) big else total
    }

    private fun detectBigCoreCount(): Int = runCatching {
        val cpuDirs = File("/sys/devices/system/cpu").listFiles { file ->
            file.isDirectory && CPU_DIR_REGEX.matches(file.name)
        }
        if (cpuDirs.isNullOrEmpty()) return@runCatching 0

        val freqs = cpuDirs.map { dir ->
            File(dir, "cpufreq/cpuinfo_max_freq").readText().trim().toLong()
        }
        val maxFreq = freqs.max()
        if (maxFreq <= 0L) return@runCatching 0

        freqs.count { it * 100 >= maxFreq * BIG_CORE_PERCENT }
    }.getOrDefault(0)
}
