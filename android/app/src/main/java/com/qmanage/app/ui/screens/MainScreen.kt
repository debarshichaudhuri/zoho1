package com.qmanage.app.ui.screens

import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.*
import com.qmanage.app.viewmodel.AppViewModel

sealed class BottomNavItem(val route: String, val label: String, val icon: ImageVector) {
    object Home       : BottomNavItem("home",       "Home",       Icons.Default.Home)
    object Invoices   : BottomNavItem("invoices",   "Invoices",   Icons.Default.ReceiptLong)
    object Deals      : BottomNavItem("deals",      "CRM",        Icons.Default.TrendingUp)
    object Contacts   : BottomNavItem("contacts",   "Contacts",   Icons.Default.People)
    object Settings   : BottomNavItem("settings",   "Settings",   Icons.Default.Settings)
}

private val bottomItems = listOf(
    BottomNavItem.Home,
    BottomNavItem.Invoices,
    BottomNavItem.Deals,
    BottomNavItem.Contacts,
    BottomNavItem.Settings
)

@Composable
fun MainScreen(vm: AppViewModel) {
    val navController = rememberNavController()
    val snackbarHost = remember { SnackbarHostState() }

    // Show toast messages
    LaunchedEffect(Unit) {
        vm.toast.collect { msg ->
            snackbarHost.showSnackbar(msg, duration = SnackbarDuration.Short)
        }
    }

    Scaffold(
        snackbarHost = { SnackbarHost(snackbarHost) },
        bottomBar = {
            NavigationBar {
                val navBackStack by navController.currentBackStackEntryAsState()
                val currentDest = navBackStack?.destination
                bottomItems.forEach { item ->
                    NavigationBarItem(
                        selected = currentDest?.hierarchy?.any { it.route == item.route } == true,
                        onClick = {
                            navController.navigate(item.route) {
                                popUpTo(navController.graph.findStartDestination().id) {
                                    saveState = true
                                }
                                launchSingleTop = true
                                restoreState = true
                            }
                        },
                        icon = { Icon(item.icon, contentDescription = item.label) },
                        label = { Text(item.label) }
                    )
                }
            }
        }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = BottomNavItem.Home.route,
            modifier = Modifier.padding(innerPadding)
        ) {
            composable(BottomNavItem.Home.route)       { DashboardScreen(vm) }
            composable(BottomNavItem.Invoices.route)   { InvoicesScreen(vm) }
            composable(BottomNavItem.Deals.route)      { DealsScreen(vm) }
            composable(BottomNavItem.Contacts.route)   { ContactsScreen(vm) }
            composable(BottomNavItem.Settings.route)   { SettingsScreen(vm) }
        }
    }
}
