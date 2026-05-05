package com.qmanage.app.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.qmanage.app.data.model.Invoice
import com.qmanage.app.viewmodel.AppViewModel
import com.qmanage.app.viewmodel.formatRupees
import java.text.SimpleDateFormat
import java.util.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InvoicesScreen(vm: AppViewModel) {
    val state by vm.invoices.collectAsState()
    val contacts by vm.contacts.collectAsState()
    var showCreate by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        vm.loadInvoices(reset = true)
        vm.loadContacts(reset = true)
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Invoices", fontWeight = FontWeight.SemiBold) },
                actions = {
                    IconButton(onClick = { vm.loadInvoices(reset = true) }) {
                        Icon(Icons.Default.Refresh, "Refresh")
                    }
                }
            )
        },
        floatingActionButton = {
            FloatingActionButton(onClick = { showCreate = true }) {
                Icon(Icons.Default.Add, "New Invoice")
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
        } else {
            LazyColumn(
                modifier = Modifier.padding(padding),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(state.items) { inv ->
                    InvoiceCard(inv)
                }
                if (state.isLoading) {
                    item { Box(Modifier.fillMaxWidth(), Alignment.Center) { CircularProgressIndicator() } }
                }
            }
        }
    }

    if (showCreate) {
        CreateInvoiceDialog(
            contacts = contacts.items.map { Pair(it.id, it.displayName) },
            onDismiss = { showCreate = false },
            onCreate = { num, custId, total, date, notes ->
                vm.createInvoice(num, custId, total, date, notes)
                showCreate = false
            }
        )
    }
}

@Composable
private fun InvoiceCard(inv: Invoice) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.padding(16.dp).fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(inv.invoiceNum, fontWeight = FontWeight.SemiBold)
                Text(inv.customerName, style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                inv.date.takeIf { it > 0 }?.let { ts ->
                    Text(
                        SimpleDateFormat("dd MMM yyyy", Locale.getDefault())
                            .format(Date(ts * 1000)),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            Column(horizontalAlignment = Alignment.End) {
                Text(inv.total.formatRupees(), fontWeight = FontWeight.Bold)
                if (inv.balanceDue > 0 && inv.status != "paid") {
                    Text("Due: ${inv.balanceDue.formatRupees()}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error)
                }
                Spacer(Modifier.height(4.dp))
                StatusBadge(inv.status)
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CreateInvoiceDialog(
    contacts: List<Pair<Long, String>>,
    onDismiss: () -> Unit,
    onCreate: (String, Long, Double, Long, String) -> Unit
) {
    var invoiceNum by remember { mutableStateOf("INV-${System.currentTimeMillis() % 100000}") }
    var selectedContact by remember { mutableStateOf(contacts.firstOrNull()) }
    var totalStr by remember { mutableStateOf("") }
    var notes by remember { mutableStateOf("") }
    var expanded by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("New Invoice") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedTextField(
                    value = invoiceNum,
                    onValueChange = { invoiceNum = it },
                    label = { Text("Invoice #") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )

                ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
                    OutlinedTextField(
                        value = selectedContact?.second ?: "Select Customer",
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Customer") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                        modifier = Modifier.menuAnchor().fillMaxWidth()
                    )
                    ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                        contacts.forEach { (id, name) ->
                            DropdownMenuItem(text = { Text(name) }, onClick = {
                                selectedContact = Pair(id, name)
                                expanded = false
                            })
                        }
                    }
                }

                OutlinedTextField(
                    value = totalStr,
                    onValueChange = { totalStr = it },
                    label = { Text("Total (₹)") },
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
                    val total = totalStr.toDoubleOrNull() ?: return@TextButton
                    val custId = selectedContact?.first ?: return@TextButton
                    onCreate(invoiceNum, custId, total, System.currentTimeMillis() / 1000, notes)
                },
                enabled = totalStr.toDoubleOrNull() != null && selectedContact != null && invoiceNum.isNotBlank()
            ) { Text("Create") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        }
    )
}
