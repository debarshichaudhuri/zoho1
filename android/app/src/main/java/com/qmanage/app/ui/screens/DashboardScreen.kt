package com.qmanage.app.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.qmanage.app.data.model.DashboardResponse
import com.qmanage.app.ui.theme.QGreen
import com.qmanage.app.ui.theme.QRed
import com.qmanage.app.ui.theme.QYellow
import com.qmanage.app.viewmodel.AppViewModel
import com.qmanage.app.viewmodel.formatRupees

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(vm: AppViewModel) {
    val state by vm.dashboard.collectAsState()

    LaunchedEffect(Unit) { vm.loadDashboard() }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Dashboard", fontWeight = FontWeight.SemiBold) },
                actions = {
                    IconButton(onClick = { vm.loadDashboard() }) {
                        Icon(Icons.Default.Refresh, "Refresh")
                    }
                }
            )
        }
    ) { padding ->
        when {
            state.isLoading -> Box(Modifier.fillMaxSize().padding(padding), Alignment.Center) {
                CircularProgressIndicator()
            }
            state.error.isNotEmpty() -> Box(Modifier.fillMaxSize().padding(padding), Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(state.error, color = MaterialTheme.colorScheme.error)
                    Spacer(Modifier.height(8.dp))
                    Button(onClick = { vm.loadDashboard() }) { Text("Retry") }
                }
            }
            state.data != null -> DashboardContent(state.data!!, Modifier.padding(padding))
        }
    }
}

@Composable
private fun DashboardContent(data: DashboardResponse, modifier: Modifier) {
    LazyColumn(
        modifier = modifier.fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // P&L Summary
        item {
            Text("Financial Summary", style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                MetricCard("Income", data.totalIncome.toLong().formatRupees(), QGreen,
                    Modifier.weight(1f))
                MetricCard("Expenses", data.totalExpenses.toLong().formatRupees(), QRed,
                    Modifier.weight(1f))
            }
            Spacer(Modifier.height(8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                val profitColor = if (data.netProfit >= 0) QGreen else QRed
                MetricCard("Net Profit", data.netProfit.toLong().formatRupees(), profitColor,
                    Modifier.weight(1f))
                MetricCard("Total Assets", data.totalAssets.toLong().formatRupees(),
                    MaterialTheme.colorScheme.primary, Modifier.weight(1f))
            }
        }

        // Outstanding
        item {
            Spacer(Modifier.height(4.dp))
            Text("Outstanding", style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                MetricCard(
                    label = "Receivable",
                    value = data.counts.totalReceivable.formatRupees(),
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.weight(1f),
                    subtitle = "${data.counts.unpaidInvoices} invoices"
                )
                MetricCard(
                    label = "Payable",
                    value = data.counts.totalPayable.formatRupees(),
                    color = QRed,
                    modifier = Modifier.weight(1f),
                    subtitle = "${data.counts.unpaidBills} bills"
                )
            }
        }

        // Counts
        item {
            Spacer(Modifier.height(4.dp))
            Text("Overview", style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                CountChip("Contacts", data.counts.totalContacts, Modifier.weight(1f))
                CountChip("Deals", data.counts.activeDeals, Modifier.weight(1f))
                CountChip("Low Stock", data.counts.lowStockItems, Modifier.weight(1f),
                    if (data.counts.lowStockItems > 0) QYellow else null)
            }
        }

        // Recent Invoices
        if (data.recentInvoices.isNotEmpty()) {
            item {
                Spacer(Modifier.height(4.dp))
                Text("Recent Invoices", style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold)
            }
            items(data.recentInvoices) { inv ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(
                        modifier = Modifier.padding(12.dp).fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column {
                            Text(inv.invoiceNum, style = MaterialTheme.typography.bodyMedium,
                                fontWeight = FontWeight.Medium)
                            Text(inv.customer, style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        Column(horizontalAlignment = Alignment.End) {
                            Text(inv.total.formatRupees(), style = MaterialTheme.typography.bodyMedium,
                                fontWeight = FontWeight.SemiBold)
                            StatusBadge(inv.status)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun MetricCard(
    label: String,
    value: String,
    color: Color,
    modifier: Modifier = Modifier,
    subtitle: String = ""
) {
    Card(
        modifier = modifier,
        elevation = CardDefaults.cardElevation(defaultElevation = 0.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface
        ),
        border = androidx.compose.foundation.BorderStroke(
            width = 1.dp,
            color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)
        )
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(label, style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(6.dp))
            Text(value, style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold, color = color)
            if (subtitle.isNotEmpty()) {
                Spacer(Modifier.height(2.dp))
                Text(subtitle, style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun CountChip(label: String, count: Long, modifier: Modifier, color: Color? = null) {
    Card(
        modifier = modifier,
        elevation = CardDefaults.cardElevation(defaultElevation = 0.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface
        ),
        border = androidx.compose.foundation.BorderStroke(
            width = 1.dp,
            color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)
        )
    ) {
        Column(modifier = Modifier.padding(16.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(count.toString(), style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
                color = color ?: MaterialTheme.colorScheme.primary)
            Text(label, style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
fun StatusBadge(status: String) {
    val (bgColor, textColor) = when (status.lowercase()) {
        "paid"     -> Pair(QGreen.copy(alpha = 0.15f), QGreen)
        "partial"  -> Pair(QYellow.copy(alpha = 0.15f), QYellow)
        "overdue"  -> Pair(QRed.copy(alpha = 0.15f), QRed)
        "void"     -> Pair(Color.Gray.copy(alpha = 0.15f), Color.Gray)
        else       -> Pair(MaterialTheme.colorScheme.primaryContainer, MaterialTheme.colorScheme.primary)
    }
    Surface(shape = MaterialTheme.shapes.small, color = bgColor) {
        Text(
            status.replaceFirstChar { it.uppercase() },
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp),
            style = MaterialTheme.typography.labelSmall,
            color = textColor
        )
    }
}
