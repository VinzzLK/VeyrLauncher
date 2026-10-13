package com.movtery.zalithlauncher.ui.theme

import android.content.Context
import android.os.PowerManager
import android.provider.Settings
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import com.movtery.zalithlauncher.setting.AllSettings
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sin

/*
 * Veyr theme: a black and blue "HyperOS" style look.
 *  - veyrDark: near-black navy surfaces with light and deep blue accents
 *  - veyrShapes: large rounded corners
 *  - VeyrAurora: slowly drifting blue light behind the launcher UI (launcher screens only, never the game)
 */

/**
 * True while the Veyr aurora backdrop sits behind the launcher UI.
 * Cards and the main surface turn translucent so the glow shows through.
 */
val LocalVeyrBackdrop = compositionLocalOf { false }

val veyrDark = darkColorScheme(
    primary = Color(0xFF7DB7FF),
    onPrimary = Color(0xFF00264D),
    primaryContainer = Color(0xFF0B3D91),
    onPrimaryContainer = Color(0xFFD6E6FF),
    secondary = Color(0xFF9AB8E8),
    onSecondary = Color(0xFF0E2547),
    secondaryContainer = Color(0xFF1B3358),
    onSecondaryContainer = Color(0xFFD9E5FF),
    tertiary = Color(0xFF5FD4FF),
    onTertiary = Color(0xFF00344A),
    tertiaryContainer = Color(0xFF0B4A66),
    onTertiaryContainer = Color(0xFFC6EEFF),
    error = Color(0xFFFF8A8A),
    onError = Color(0xFF5C0A0A),
    errorContainer = Color(0xFF7A1F1F),
    onErrorContainer = Color(0xFFFFDAD6),
    background = Color(0xFF03060D),
    onBackground = Color(0xFFE6EDFB),
    surface = Color(0xFF060B16),
    onSurface = Color(0xFFE6EDFB),
    surfaceVariant = Color(0xFF152238),
    onSurfaceVariant = Color(0xFFB5C4DE),
    outline = Color(0xFF5C6F90),
    outlineVariant = Color(0xFF26354F),
    scrim = Color(0xFF000000),
    inverseSurface = Color(0xFFE6EDFB),
    inverseOnSurface = Color(0xFF0B1424),
    inversePrimary = Color(0xFF1A5BC4),
    surfaceDim = Color(0xFF03060D),
    surfaceBright = Color(0xFF1B2A44),
    surfaceContainerLowest = Color(0xFF02050B),
    surfaceContainerLow = Color(0xFF08101F),
    surfaceContainer = Color(0xFF0B1527),
    surfaceContainerHigh = Color(0xFF111E36),
    surfaceContainerHighest = Color(0xFF182845),
)

val veyrShapes = Shapes(
    extraSmall = RoundedCornerShape(10.dp),
    small = RoundedCornerShape(14.dp),
    medium = RoundedCornerShape(20.dp),
    large = RoundedCornerShape(26.dp),
    extraLarge = RoundedCornerShape(34.dp)
)

/**
 * Black to navy gradient with three slowly drifting blue glows.
 * The animation only runs while the launcher is on screen: it freezes in the background (so it never
 * costs anything while the game is running), in Battery Saver, and when system animations are off.
 */
@Composable
fun VeyrAurora(modifier: Modifier = Modifier) {
    val animated = AllSettings.veyrAnimatedBackground.state && rememberVeyrMotionAllowed()

    if (animated) {
        val transition = rememberInfiniteTransition(label = "veyrAurora")
        val phase by transition.animateFloat(
            initialValue = 0f,
            targetValue = 1f,
            animationSpec = infiniteRepeatable(
                animation = tween(durationMillis = 36000, easing = LinearEasing),
                repeatMode = RepeatMode.Restart
            ),
            label = "veyrAuroraPhase"
        )
        // The state is read inside the draw block, so every frame only redraws and never recomposes.
        Canvas(modifier = modifier.fillMaxSize()) {
            drawVeyrAurora(phase)
        }
    } else {
        Canvas(modifier = modifier.fillMaxSize()) {
            drawVeyrAurora(0.18f)
        }
    }
}

@Composable
private fun rememberVeyrMotionAllowed(): Boolean {
    val context = LocalContext.current
    var resumed by remember { mutableStateOf(true) }
    var batterySaver by remember { mutableStateOf(isBatterySaver(context)) }

    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) {
        resumed = true
        batterySaver = isBatterySaver(context)
    }
    LifecycleEventEffect(Lifecycle.Event.ON_PAUSE) {
        resumed = false
    }

    val animationsEnabled = remember(resumed) {
        Settings.Global.getFloat(context.contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1f) > 0f
    }
    return resumed && !batterySaver && animationsEnabled
}

private fun isBatterySaver(context: Context): Boolean {
    val powerManager = context.getSystemService(Context.POWER_SERVICE) as? PowerManager
    return powerManager?.isPowerSaveMode == true
}

private fun DrawScope.drawVeyrAurora(phase: Float) {
    val width = size.width
    val height = size.height
    if (width <= 0f || height <= 0f) return

    drawRect(
        brush = Brush.verticalGradient(
            colors = listOf(Color(0xFF02040A), Color(0xFF050B1A), Color(0xFF07122A))
        )
    )

    // Every term below is an integer multiple of the phase angle, so the loop has no visible seam.
    val angle = phase * 2f * PI.toFloat()
    val reach = max(width, height)

    drawAuroraBlob(
        center = Offset(width * (0.22f + 0.10f * sin(angle)), height * (0.28f + 0.10f * cos(angle))),
        radius = reach * 0.62f,
        color = Color(0xFF1F6BFF),
        alpha = 0.34f
    )
    drawAuroraBlob(
        center = Offset(width * (0.80f + 0.08f * cos(angle + 1.3f)), height * (0.20f + 0.12f * sin(2f * angle))),
        radius = reach * 0.52f,
        color = Color(0xFF63D2FF),
        alpha = 0.20f
    )
    drawAuroraBlob(
        center = Offset(width * (0.55f + 0.14f * sin(angle + 2.4f)), height * (0.92f + 0.06f * cos(2f * angle))),
        radius = reach * 0.66f,
        color = Color(0xFF0A3FA6),
        alpha = 0.42f
    )
}

private fun DrawScope.drawAuroraBlob(center: Offset, radius: Float, color: Color, alpha: Float) {
    drawCircle(
        brush = Brush.radialGradient(
            colors = listOf(color.copy(alpha = alpha), Color.Transparent),
            center = center,
            radius = radius
        ),
        radius = radius,
        center = center
    )
}
