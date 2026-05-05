@file:OptIn(ExperimentalLayoutApi::class)
package com.qmanage.app.ui.components

import androidx.compose.animation.*
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.keyframes
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.*
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

// Message types
sealed class ChatMessage(
    open val text: String,
    open val timestamp: Long
) {
    data class User(
        override val text: String,
        override val timestamp: Long = java.lang.System.currentTimeMillis()
    ) : ChatMessage(text, timestamp)
    
    data class Bot(
        override val text: String,
        val isVerified: Boolean = false,
        val dataSource: String? = null,
        override val timestamp: Long = java.lang.System.currentTimeMillis()
    ) : ChatMessage(text, timestamp)
    
    data class System(
        override val text: String,
        override val timestamp: Long = java.lang.System.currentTimeMillis()
    ) : ChatMessage(text, timestamp)
}

@Composable
fun DraggableChatbotFab(
    modifier: Modifier = Modifier
) {
    var isOpen by remember { mutableStateOf(false) }
    var fabPosition by remember { mutableStateOf(androidx.compose.ui.geometry.Offset(100f, 100f)) }
    var isDragging by remember { mutableStateOf(false) }
    
    Box(modifier = modifier.fillMaxSize()) {
        // Chat Dialog
        if (isOpen) {
            ChatDialog(
                onDismiss = { isOpen = false }
            )
        }
        
        // Floating Action Button (Draggable)
        Surface(
            onClick = { isOpen = true },
            modifier = Modifier
                .offset { IntOffset(fabPosition.x.roundToInt(), fabPosition.y.roundToInt()) }
                .size(56.dp)
                .pointerInput(Unit) {
                    detectDragGestures(
                        onDragStart = { isDragging = true },
                        onDragEnd = { isDragging = false },
                        onDrag = { change, dragAmount ->
                            change.consume()
                            fabPosition = androidx.compose.ui.geometry.Offset(
                                fabPosition.x + dragAmount.x,
                                fabPosition.y + dragAmount.y
                            )
                        }
                    )
                }
                .clip(CircleShape),
            color = MaterialTheme.colorScheme.primary,
            tonalElevation = 6.dp,
            shadowElevation = 8.dp
        ) {
            Box(contentAlignment = Alignment.Center) {
                Icon(
                    if (isOpen) Icons.Filled.Close else Icons.Filled.Chat,
                    contentDescription = if (isOpen) "Close Chat" else "Open Chat",
                    tint = MaterialTheme.colorScheme.onPrimary
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ChatDialog(
    onDismiss: () -> Unit
) {
    var messages by remember { mutableStateOf(listOf<ChatMessage>(
        ChatMessage.System("Welcome! I'm your Q Manage AI assistant. I can help you with:")
    )) }
    var inputText by remember { mutableStateOf("") }
    var isVoiceMode by remember { mutableStateOf(false) }
    var isListening by remember { mutableStateOf(false) }
    var isSpeaking by remember { mutableStateOf(false) }
    var showSuggestions by remember { mutableStateOf(true) }
    
    val listState = rememberLazyListState()
    val scope = rememberCoroutineScope()
    
    // Auto-scroll to bottom
    LaunchedEffect(messages.size) {
        if (messages.isNotEmpty()) {
            listState.animateScrollToItem(messages.size - 1)
        }
    }
    
    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(
            usePlatformDefaultWidth = false,
            decorFitsSystemWindows = false
        )
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.85f)
                .padding(16.dp),
            shape = RoundedCornerShape(28.dp),
            tonalElevation = 6.dp,
            shadowElevation = 8.dp
        ) {
            Column(modifier = Modifier.fillMaxSize()) {
                // Header
                Surface(
                    color = MaterialTheme.colorScheme.primaryContainer,
                    shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(12.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Surface(
                                shape = CircleShape,
                                color = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.size(40.dp)
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Icon(
                                        Icons.Filled.SmartToy,
                                        contentDescription = null,
                                        tint = MaterialTheme.colorScheme.onPrimary
                                    )
                                }
                            }
                            Column {
                                Text(
                                    "Q AI Assistant",
                                    style = MaterialTheme.typography.titleMedium,
                                    fontWeight = FontWeight.SemiBold
                                )
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    Surface(
                                        shape = CircleShape,
                                        color = MaterialTheme.colorScheme.tertiaryContainer,
                                        modifier = Modifier.size(8.dp)
                                    ) {}
                                    Text(
                                        "Local AI • Offline",
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )
                                }
                            }
                        }
                        
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            // Voice output toggle
                            IconButton(
                                onClick = { isSpeaking = !isSpeaking }
                            ) {
                                Icon(
                                    if (isSpeaking) Icons.Filled.VolumeUp else Icons.Filled.VolumeOff,
                                    contentDescription = if (isSpeaking) "TTS On" else "TTS Off",
                                    tint = if (isSpeaking) 
                                        MaterialTheme.colorScheme.primary 
                                    else 
                                        MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                            IconButton(onClick = onDismiss) {
                                Icon(Icons.Filled.Close, "Close")
                            }
                        }
                    }
                }
                
                HorizontalDivider()
                
                // Messages
                LazyColumn(
                    modifier = Modifier.weight(1f),
                    state = listState,
                    contentPadding = PaddingValues(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    items(messages) { message ->
                        when (message) {
                            is ChatMessage.User -> UserMessage(message)
                            is ChatMessage.Bot -> BotMessage(message)
                            is ChatMessage.System -> SystemMessage(message)
                        }
                    }
                    
                    if (isListening) {
                        item {
                            ListeningIndicator()
                        }
                    }
                }
                
                // Suggestions
                if (showSuggestions && messages.size < 3) {
                    SuggestionsRow(
                        onSuggestion = { suggestion ->
                            messages = messages + ChatMessage.User(suggestion)
                            showSuggestions = false
                            // Simulate AI response
                            scope.launch {
                                kotlinx.coroutines.delay(500)
                                val response = getAIResponse(suggestion)
                                messages = messages + response
                            }
                        }
                    )
                }
                
                HorizontalDivider()
                
                // Input area
                Surface(
                    tonalElevation = 2.dp
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        horizontalArrangement = Arrangement.spacedBy(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        // Voice/Mode toggle
                        IconButton(
                            onClick = { 
                                isVoiceMode = !isVoiceMode
                                if (isVoiceMode) {
                                    isListening = true
                                }
                            }
                        ) {
                            Icon(
                                if (isVoiceMode) Icons.Filled.Mic else Icons.Filled.Keyboard,
                                contentDescription = if (isVoiceMode) "Voice Mode" else "Text Mode",
                                tint = if (isVoiceMode) 
                                    MaterialTheme.colorScheme.primary 
                                else 
                                    MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        
                        if (isVoiceMode && isListening) {
                            // Voice input active
                            Surface(
                                shape = RoundedCornerShape(24.dp),
                                color = MaterialTheme.colorScheme.primaryContainer,
                                modifier = Modifier.weight(1f)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    Icon(
                                        Icons.Default.Mic,
                                        contentDescription = null,
                                        tint = MaterialTheme.colorScheme.primary
                                    )
                                    Text(
                                        "Listening...",
                                        color = MaterialTheme.colorScheme.onPrimaryContainer
                                    )
                                    Spacer(Modifier.weight(1f))
                                    // Waveform animation dots
                                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                        repeat(3) { index ->
                                            val alpha by animateFloatAsState(
                                                targetValue = if (isListening) 1f else 0.3f,
                                                animationSpec = infiniteRepeatable(
                                                    keyframes {
                                                        durationMillis = 600
                                                        0.3f at (index * 200)
                                                        1f at (index * 200 + 100)
                                                        0.3f at (index * 200 + 200)
                                                    }
                                                ),
                                                label = "waveform"
                                            )
                                            Box(
                                                modifier = Modifier
                                                    .size(6.dp)
                                                    .clip(CircleShape)
                                                    .background(
                                                        MaterialTheme.colorScheme.primary.copy(alpha = alpha)
                                                    )
                                            )
                                        }
                                    }
                                }
                            }
                            
                            IconButton(
                                onClick = { 
                                    isListening = false
                                    isVoiceMode = false
                                }
                            ) {
                                Icon(Icons.Filled.Stop, "Stop")
                            }
                        } else {
                            // Text input
                            OutlinedTextField(
                                value = inputText,
                                onValueChange = { 
                                    inputText = it
                                    showSuggestions = false
                                },
                                placeholder = { Text("Ask about your data...") },
                                modifier = Modifier.weight(1f),
                                singleLine = true,
                                colors = OutlinedTextFieldDefaults.colors(
                                    focusedContainerColor = Color.Transparent,
                                    unfocusedContainerColor = Color.Transparent
                                )
                            )
                            
                            IconButton(
                                onClick = {
                                    if (inputText.isNotBlank()) {
                                        val query = inputText
                                        messages = messages + ChatMessage.User(query)
                                        inputText = ""
                                        showSuggestions = false
                                        // Simulate AI response
                                        scope.launch {
                                            kotlinx.coroutines.delay(800)
                                            val response = getAIResponse(query)
                                            messages = messages + response
                                        }
                                    }
                                },
                                enabled = inputText.isNotBlank()
                            ) {
                                Icon(Icons.AutoMirrored.Filled.Send, "Send")
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun UserMessage(message: ChatMessage.User) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.End
    ) {
        Surface(
            color = MaterialTheme.colorScheme.primaryContainer,
            shape = RoundedCornerShape(
                topStart = 20.dp,
                topEnd = 4.dp,
                bottomStart = 20.dp,
                bottomEnd = 20.dp
            ),
            modifier = Modifier.padding(start = 48.dp)
        ) {
            Text(
                text = message.text,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
                color = MaterialTheme.colorScheme.onPrimaryContainer
            )
        }
    }
}

@Composable
private fun BotMessage(message: ChatMessage.Bot) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Surface(
            shape = CircleShape,
            color = MaterialTheme.colorScheme.tertiaryContainer,
            modifier = Modifier.size(32.dp)
        ) {
            Box(contentAlignment = Alignment.Center) {
                Icon(
                    Icons.Filled.SmartToy,
                    contentDescription = null,
                    modifier = Modifier.size(20.dp),
                    tint = MaterialTheme.colorScheme.onTertiaryContainer
                )
            }
        }
        
        Column {
            Surface(
                color = MaterialTheme.colorScheme.surfaceVariant,
                shape = RoundedCornerShape(
                    topStart = 4.dp,
                    topEnd = 20.dp,
                    bottomStart = 20.dp,
                    bottomEnd = 20.dp
                ),
                modifier = Modifier.padding(end = 48.dp)
            ) {
                Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp)) {
                    Text(
                        text = message.text,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    
                    if (message.isVerified) {
                        Spacer(Modifier.height(4.dp))
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Icon(
                                Icons.Filled.Verified,
                                contentDescription = null,
                                modifier = Modifier.size(14.dp),
                                tint = MaterialTheme.colorScheme.tertiary
                            )
                            Text(
                                "Verified from ${message.dataSource}",
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.tertiary
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SystemMessage(message: ChatMessage.System) {
    Surface(
        color = MaterialTheme.colorScheme.surface,
        shape = RoundedCornerShape(12.dp),
        modifier = Modifier.fillMaxWidth()
    ) {
        Text(
            text = message.text,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun ListeningIndicator() {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center
    ) {
        Surface(
            color = MaterialTheme.colorScheme.primaryContainer,
            shape = RoundedCornerShape(20.dp)
        ) {
            Row(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    Icons.Filled.Mic,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary
                )
                Text(
                    "Listening...",
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }
    }
}

@Composable
private fun SuggestionsRow(onSuggestion: (String) -> Unit) {
    val suggestions = listOf(
        "Show my income this month",
        "Who owes me money?",
        "Create an invoice",
        "How do I add a contact?",
        "What's my net profit?"
    )
    
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp)
    ) {
        Text(
            "Try asking:",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 8.dp)
        )
        
        FlowRow(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            suggestions.forEach { suggestion ->
                SuggestionChip(
                    onClick = { onSuggestion(suggestion) },
                    label = { Text(suggestion) }
                )
            }
        }
    }
}

// Simple AI response simulator
private fun getAIResponse(query: String): ChatMessage.Bot {
    val lowerQuery = query.lowercase()
    
    return when {
        lowerQuery.contains("income") || lowerQuery.contains("revenue") ->
            ChatMessage.Bot(
                text = "Your total income this month is ₹25,800. This includes:\n\n• Invoice #INV-11438: ₹258.00\n• Consulting fees: ₹15,000\n• Product sales: ₹10,542\n\nWould you like to see a breakdown by category?",
                isVerified = true,
                dataSource = "invoices, transactions"
            )
        
        lowerQuery.contains("owe") || lowerQuery.contains("receivable") || lowerQuery.contains("pending") ->
            ChatMessage.Bot(
                text = "You have 1 outstanding invoice:\n\n• INV-11438 (test): ₹258.00 - Due: 06 May 2026\n\nTotal receivable: ₹258.00",
                isVerified = true,
                dataSource = "invoices"
            )
        
        lowerQuery.contains("profit") || lowerQuery.contains("net") ->
            ChatMessage.Bot(
                text = "Your net profit is ₹25,800 (100% margin).\n\nIncome: ₹25,800\nExpenses: ₹0\n\nGreat job keeping expenses low!",
                isVerified = true,
                dataSource = "dashboard"
            )
        
        lowerQuery.contains("contact") || lowerQuery.contains("add") ->
            ChatMessage.Bot(
                text = "To add a contact:\n\n1. Go to Contacts tab\n2. Tap + button\n3. Enter name, email, phone\n4. Select type (Customer/Supplier)\n5. Save\n\nWould you like me to help with something else?",
                isVerified = true,
                dataSource = "app documentation"
            )
        
        lowerQuery.contains("invoice") || lowerQuery.contains("create") ->
            ChatMessage.Bot(
                text = "I can help you create an invoice. Here are steps:\n\n1. Go to Invoices tab\n2. Tap + button\n3. Select customer\n4. Add line items\n5. Set due date\n6. Save & Send\n\nWould you like me to walk you through each step?",
                isVerified = true,
                dataSource = "app documentation"
            )
        
        else ->
            ChatMessage.Bot(
                text = "I understand you're asking about \"$query\". I can help you with:\n\n• Financial summaries and reports\n• Invoice and payment tracking\n• Contact management\n• App navigation and features\n\nWhat specific information do you need?",
                isVerified = false
            )
    }
}
