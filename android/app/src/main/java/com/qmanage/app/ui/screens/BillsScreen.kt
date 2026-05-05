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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.qmanage.app.data.model.Bill
import com.qmanage.app.viewmodel.AppViewModel
import com.qmanage.app.viewmodel.formatRupees
import java.text.SimpleDateFormat
import java.util.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BillsScreen(vm: AppViewModel) {
    val state by vm.bills.collectAsState()

    LaunchedEffect(Unit) { vm.loadBills(reset = true) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Bills", fontWeight = FontWeight.SemiBold) },
                actions = {
                    IconButton(onClick = { vm.loadBills(reset = true) }) {
                        Icon(Icons.Default.Refresh, "Refresh")
                    }
                }
            )
        }
    ) { padding ->
        if (state.isLoading && state.items.isEmpty()) {
            Box(Modifier.fillMaxSize().padding(padding), Alignment.Center) {
                CircularProgressIndicator()
            }
        } else {
            LazyColumn(
                modifier = Modifier.padding(padding),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(state.items) { bill -> BillCard(bill) }
            }
        }
    }
}

@Composable
private fun BillCard(bill: Bill) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.padding(16.dp).fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(bill.billNum, fontWeight = FontWeight.SemiBold)
                Text(bill.vendorName, style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                bill.date.takeIf { it > 0 }?.let { ts ->
                    Text(
                        SimpleDateFormat("dd MMM yyyy", Locale.getDefault()).format(Date(ts * 1000)),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            Column(horizontalAlignment = Alignment.End) {
                Text(bill.total.formatRupees(), fontWeight = FontWeight.Bold)
                if (bill.balanceDue > 0) {
                    Text("Due: ${bill.balanceDue.formatRupees()}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error)
                }
                Spacer(Modifier.height(4.dp))
                StatusBadge(bill.status)
            }
        }
    }
}
