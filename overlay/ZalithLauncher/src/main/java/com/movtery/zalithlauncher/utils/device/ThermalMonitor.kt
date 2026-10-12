package com.movtery.zalithlauncher.utils.device

import android.content.Context
import android.os.Build
import android.os.PowerManager
import androidx.annotation.RequiresApi
import com.movtery.zalithlauncher.utils.logging.Logger
import java.util.concurrent.Executor

private const val TAG = "ThermalMonitor"

/**
 * Passive thermal status logger (Veyr, Android 10+).
 *
 * It only records the status in the launcher log so throttling can be correlated with FPS drops.
 * It never lowers quality or caps FPS by itself, and it ignores battery level on purpose.
 */
object ThermalMonitor {
    @Volatile
    var status: Int = 0
        private set

    private var started = false

    fun start(context: Context) {
        if (started || Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        started = true
        runCatching {
            register(context)
        }.onFailure {
            Logger.warning(TAG, "Unable to register the thermal status listener.", it)
        }
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun register(context: Context) {
        val powerManager = context.getSystemService(Context.POWER_SERVICE) as? PowerManager ?: return
        status = powerManager.currentThermalStatus
        Logger.info(TAG, "Initial thermal status: ${statusName(status)}")
        powerManager.addThermalStatusListener(Executor { it.run() }) { newStatus ->
            status = newStatus
            Logger.info(TAG, "Thermal status changed: ${statusName(newStatus)}")
        }
    }

    private fun statusName(status: Int): String = when (status) {
        0 -> "NONE"
        1 -> "LIGHT"
        2 -> "MODERATE"
        3 -> "SEVERE"
        4 -> "CRITICAL"
        5 -> "EMERGENCY"
        6 -> "SHUTDOWN"
        else -> "UNKNOWN($status)"
    }
}
