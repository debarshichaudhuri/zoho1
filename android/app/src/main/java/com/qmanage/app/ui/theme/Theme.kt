package com.qmanage.app.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

// Q Manage brand colors
val QBlue       = Color(0xFF1A73E8)
val QBlueContainer = Color(0xFFD2E3FC)
val QBlueDark    = Color(0xFF1557B0)
val QGreen      = Color(0xFF34A853)
val QGreenContainer = Color(0xFFD7EED9)
val QRed        = Color(0xFFEA4335)
val QYellow     = Color(0xFFFBBC04)
val QPurple     = Color(0xFF9333EA)
val QPurpleContainer = Color(0xFFE9D5FF)
val QSurface    = Color(0xFFF1F3F4)
val QCard       = Color(0xFFFFFFFF)

private val DarkColorScheme = darkColorScheme(
    primary = Color(0xFF8AB4F8),  // Lighter blue for dark mode
    onPrimary = Color(0xFF0D2E6E),  // Dark blue for contrast
    primaryContainer = Color(0xFF1E3A8A),  // Medium blue container
    onPrimaryContainer = Color(0xFFD2E3FC),  // Light blue text on container
    secondary = Color(0xFF81C995),  // Lighter green
    onSecondary = Color(0xFF0D3822),  // Dark green for contrast
    secondaryContainer = Color(0xFF1E3829),
    onSecondaryContainer = Color(0xFFD7EED9),
    tertiary = Color(0xFFD0BCFF),  // Lighter purple
    onTertiary = Color(0xFF4F378B),  // Dark purple for contrast
    tertiaryContainer = Color(0xFF6B2594),
    onTertiaryContainer = Color(0xFFE9D5FF),
    error = Color(0xFFEF9A9A),  // Lighter red
    onError = Color(0xFF4C1E1E),  // Dark red for contrast
    background = Color(0xFF121212),  // Material 3 dark background
    onBackground = Color(0xFFE1E2E1),  // Light text on background
    surface = Color(0xFF1E1E1E),  // Dark surface
    onSurface = Color(0xFFE1E2E1),  // Light text on surface
    outline = Color(0xFF6C7075),  // Lighter outline for visibility
    outlineVariant = Color(0xFF4C5052),
    scrim = Color(0xFF000000),
)

private val LightColorScheme = lightColorScheme(
    primary = QBlue,
    onPrimary = Color.White,
    primaryContainer = QBlueContainer,
    onPrimaryContainer = QBlue,
    secondary = QGreen,
    onSecondary = Color.White,
    secondaryContainer = QGreenContainer,
    onSecondaryContainer = QGreen,
    tertiary = QPurple,
    onTertiary = Color.White,
    tertiaryContainer = QPurpleContainer,
    onTertiaryContainer = QPurple,
    error = QRed,
    onError = Color.White,
    background = Color(0xFFFFFBFE),
    onBackground = Color(0xFF1C1B1F),
    surface = QCard,
    onSurface = Color(0xFF1C1B1F),
    outline = Color(0xFFDADCE0),
    outlineVariant = Color(0xFFE0E0E0),
    scrim = Color(0xFF000000),
)

@Composable
fun QManageTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val colorScheme = if (darkTheme) DarkColorScheme else LightColorScheme
    

    MaterialTheme(
        colorScheme = colorScheme,
        typography = Typography(),
        content = content
    )
}
