package com.qmanage.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.animation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.qmanage.app.ui.components.DraggableChatbotFab
import com.qmanage.app.ui.screens.*
import com.qmanage.app.ui.theme.QManageTheme
import com.qmanage.app.viewmodel.AppViewModel

class MainActivity : ComponentActivity() {
    private val vm: AppViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val isDarkTheme by vm.themePrefs.darkModeFlow.collectAsState(initial = false)
            QManageTheme(darkTheme = isDarkTheme) {
                QManageApp(vm)
            }
        }
    }
}

sealed class AppState {
    object Onboarding : AppState()
    object Auth : AppState()
    object Main : AppState()
}

@OptIn(ExperimentalAnimationApi::class)
@Composable
fun QManageApp(vm: AppViewModel) {
    val auth by vm.auth.collectAsState()
    var appState by remember { mutableStateOf<AppState>(AppState.Onboarding) }
    var hasCompletedOnboarding by remember { mutableStateOf(false) }
    
    // Check if onboarding was completed previously
    LaunchedEffect(Unit) {
        // In real app, check DataStore/Preferences
        hasCompletedOnboarding = false // Set to true to skip onboarding
        if (hasCompletedOnboarding) {
            appState = AppState.Auth
        }
    }

    AnimatedContent(
        targetState = appState,
        transitionSpec = {
            fadeIn() togetherWith fadeOut()
        },
        label = "app_state"
    ) { state ->
        when (state) {
            AppState.Onboarding -> {
                OnboardingScreen(
                    onComplete = { 
                        hasCompletedOnboarding = true
                        appState = AppState.Auth 
                    },
                    onSkip = { 
                        hasCompletedOnboarding = true
                        appState = AppState.Auth 
                    }
                )
            }
            AppState.Auth -> {
                AnimatedContent(
                    targetState = auth.isLoggedIn,
                    transitionSpec = {
                        if (targetState) {
                            slideInHorizontally { it } + fadeIn() togetherWith
                            slideOutHorizontally { -it } + fadeOut()
                        } else {
                            slideInHorizontally { -it } + fadeIn() togetherWith
                            slideOutHorizontally { it } + fadeOut()
                        }
                    },
                    label = "auth_transition"
                ) { loggedIn ->
                    if (loggedIn) {
                        appState = AppState.Main
                        MainScreenWithChatbot(vm)
                    } else {
                        LoginScreen(vm)
                    }
                }
            }
            AppState.Main -> {
                MainScreenWithChatbot(vm)
            }
        }
    }
}

@Composable
fun MainScreenWithChatbot(vm: AppViewModel) {
    Box(modifier = Modifier.fillMaxSize()) {
        MainScreen(vm)
        DraggableChatbotFab(modifier = Modifier.fillMaxSize())
    }
}
