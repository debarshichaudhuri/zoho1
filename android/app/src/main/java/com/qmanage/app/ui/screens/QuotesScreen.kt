package com.qmanage.app.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import com.qmanage.app.data.model.Quote
import com.qmanage.app.viewmodel.AppViewModel
import com.qmanage.app.viewmodel.formatRupees
import java.text.SimpleDateFormat
import java.util.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun QuotesScreen(vm: AppViewModel) {
    val state by vm.quotes.collectAsState()
    val contacts by vm.contacts.collectAsState()
    var showCreate by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        vm.loadQuotes(reset = true)
        vm.loadContacts(reset = true)
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Quotes & Estimates", fontWeight = FontWeight.SemiBold) },
                actions = {
                    IconButton(onClick = { vm.loadQuotes(reset = true) }) {
                        Icon(Icons.Default.Refresh, "Refresh")
                    }
                }
            )
        },
        floatingActionButton = {
            FloatingActionButton(onClick = { showCreate = true }) {
                Icon(Icons.Default.Add, "New Quote")
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
                Text("No quotes yet. Create one with +", color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        } else {
            LazyColumn(
                modifier = Modifier.padding(padding),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(state.items) { quote -> QuoteCard(quote) }
                if (state.isLoading) {
                    item { Box(Modifier.fillMaxWidth(), Alignment.Center) { CircularProgressIndicator() } }
                }
            }
        }
    }

    if (showCreate) {
        CreateQuoteDialog(
            contacts = contacts.items.map { Pair(it.id, it.displayName) },
            onDismiss = { showCreate = false },
            onCreate = { num, custId, total, notes ->
                vm.createQuote(num, custId, total, notes)
                showCreate = false
            }
        )
    }
}

@Composable
private fun QuoteCard(quote: Quote) {
    val statusColor = when (quote.status) {
        "accepted"  -> MaterialTheme.colorScheme.primaryContainer
        "declined"  -> MaterialTheme.colorScheme.errorContainer
        "converted" -> MaterialTheme.colorScheme.secondaryContainer
        else        -> MaterialTheme.colorScheme.surfaceVariant
    }
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.padding(16.dp).fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(quote.quoteNum, fontWeight = FontWeight.SemiBold)
                Text(quote.customerName, style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                quote.date.takeIf { it > 0 }?.let { ts ->
                    Text(
                        SimpleDateFormat("dd MMM yyyy", Locale.getDefault()).format(Date(ts * 1000)),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            Column(horizontalAlignment = Alignment.End) {
                Text(quote.total.formatRupees(), fontWeight = FontWeight.Bold)
                Spacer(Modifier.height(4.dp))
                Surface(color = statusColor, shape = MaterialTheme.shapes.small) {
                    Text(
                        quote.status.uppercase(),
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp),
                        style = MaterialTheme.typography.labelSmall,
                        fontWeight = FontWeight.SemiBold
                    )
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CreateQuoteDialog(
    contacts: List<Pair<Long, String>>,
    onDismiss: () -> Unit,
    onCreate: (String, Long, Double, String) -> Unit
) {
    var quoteNum by remember { mutableStateOf("QTE-${System.currentTimeMillis() % 100000}") }
    var selectedContact by remember { mutableStateOf(contacts.firstOrNull()) }
    var totalStr by remember { mutableStateOf("") }
    var notes by remember { mutableStateOf("") }
    var expanded by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("New Quote") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedTextField(
                    value = quoteNum,
                    onValueChange = { quoteNum = it },
                    label = { Text("Quote #") },
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
                    onCreate(quoteNum, custId, total, notes)
                },
                enabled = totalStr.toDoubleOrNull() != null && selectedContact != null && quoteNum.isNotBlank()
            ) { Text("Create") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } }
    )
}
