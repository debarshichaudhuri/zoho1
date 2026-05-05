package com.qmanage.app.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.unit.dp
import com.qmanage.app.data.model.Deal
import com.qmanage.app.viewmodel.AppViewModel
import com.qmanage.app.viewmodel.formatRupees

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DealsScreen(vm: AppViewModel) {
    val state by vm.deals.collectAsState()
    var showCreate by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) { vm.loadDeals(reset = true) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("CRM Pipeline", fontWeight = FontWeight.SemiBold) },
                actions = {
                    IconButton(onClick = { vm.loadDeals(reset = true) }) {
                        Icon(Icons.Default.Refresh, "Refresh")
                    }
                }
            )
        },
        floatingActionButton = {
            FloatingActionButton(onClick = { showCreate = true }) {
                Icon(Icons.Default.Add, "New Deal")
            }
        }
    ) { padding ->
        if (state.isLoading && state.items.isEmpty()) {
            Box(Modifier.fillMaxSize().padding(padding), Alignment.Center) {
                CircularProgressIndicator()
            }
        } else if (state.error.isNotEmpty() && state.items.isEmpty()) {
            Box(Modifier.fillMaxSize().padding(padding), Alignment.Center) {
                Text(state.error, color = MaterialTheme.colorScheme.error)
            }
        } else if (state.items.isEmpty()) {
            Box(Modifier.fillMaxSize().padding(padding), Alignment.Center) {
                Text("No deals in pipeline. Add one with +",
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        } else {
            LazyColumn(
                modifier = Modifier.padding(padding),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(state.items) { deal -> DealCard(deal) }
                if (state.isLoading) {
                    item { Box(Modifier.fillMaxWidth(), Alignment.Center) { CircularProgressIndicator() } }
                }
            }
        }
    }

    if (showCreate) {
        CreateDealDialog(
            onDismiss = { showCreate = false },
            onCreate = { title, amount, notes ->
                vm.createDeal(title, amountRupees = amount, notes = notes)
                showCreate = false
            }
        )
    }
}

@Composable
private fun DealCard(deal: Deal) {
    val (stageColor, stageBg) = when (deal.stage) {
        "contacted"   -> Pair(Color(0xFFE65100), Color(0xFFFFF3E0))
        "qualified"   -> Pair(Color(0xFF1565C0), Color(0xFFE3F2FD))
        "quoted"      -> Pair(Color(0xFF6A1B9A), Color(0xFFF3E5F5))
        "negotiating" -> Pair(Color(0xFF827717), Color(0xFFF9FBE7))
        "won"         -> Pair(Color(0xFF2E7D32), Color(0xFFE8F5E9))
        "lost"        -> Pair(Color(0xFFC62828), Color(0xFFFFEBEE))
        else          -> Pair(MaterialTheme.colorScheme.onSurface, MaterialTheme.colorScheme.surfaceVariant)
    }

    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.padding(16.dp).fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(deal.title, fontWeight = FontWeight.SemiBold)
                if (deal.contactName.isNotEmpty()) {
                    Text(deal.contactName, style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                Text("${deal.probability}% probability",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Column(horizontalAlignment = Alignment.End) {
                if (deal.amount > 0) {
                    Text(deal.amount.formatRupees(), fontWeight = FontWeight.Bold)
                    Spacer(Modifier.height(4.dp))
                }
                Surface(color = stageBg, shape = MaterialTheme.shapes.small) {
                    Text(
                        deal.stage.uppercase(),
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp),
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.SemiBold,
                        color = stageColor
                    )
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CreateDealDialog(
    onDismiss: () -> Unit,
    onCreate: (String, Double, String) -> Unit
) {
    var title by remember { mutableStateOf("") }
    var amountStr by remember { mutableStateOf("") }
    var notes by remember { mutableStateOf("") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("New Deal") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedTextField(
                    value = title,
                    onValueChange = { title = it },
                    label = { Text("Deal Title") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    placeholder = { Text("e.g. Enterprise License Q4") }
                )
                OutlinedTextField(
                    value = amountStr,
                    onValueChange = { amountStr = it },
                    label = { Text("Deal Value (₹, optional)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
                OutlinedTextField(
                    value = notes,
                    onValueChange = { notes = it },
                    label = { Text("Notes (optional)") },
                    modifier = Modifier.fillMaxWidth(),
                    maxLines = 3
                )
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val amount = amountStr.toDoubleOrNull() ?: 0.0
                    onCreate(title, amount, notes)
                },
                enabled = title.isNotBlank()
            ) { Text("Create") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } }
    )
}
