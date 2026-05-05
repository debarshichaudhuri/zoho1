package com.qmanage.app.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TermsScreen(
    onAccept: () -> Unit,
    onDecline: () -> Unit
) {
    var accepted by remember { mutableStateOf(false) }
    val scrollState = rememberScrollState()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Terms & Conditions", fontWeight = FontWeight.SemiBold) }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .fillMaxSize()
        ) {
            // Scrollable content
            Column(
                modifier = Modifier
                    .weight(1f)
                    .verticalScroll(scrollState)
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Header
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.primaryContainer
                    )
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text(
                            "Q Manage - Edge-Native ERP",
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onPrimaryContainer
                        )
                        Spacer(Modifier.height(8.dp))
                        Text(
                            "Local-First AI-Powered Business Management",
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onPrimaryContainer
                        )
                    }
                }

                // AI Disclaimer
                TermsSection(
                    title = "1. AI Assistant Disclaimer",
                    content = """
                        • The AI chatbot provides assistance based on your local data
                        • Financial advice is for informational purposes only
                        • Always verify critical financial decisions independently
                        • The AI may not have complete context for complex scenarios
                    """.trimIndent()
                )

                // Data Privacy
                TermsSection(
                    title = "2. Data Privacy & Security",
                    content = """
                        • All financial data is stored locally on your device
                        • SQLite database with AES-256-GCM encryption
                        • No cloud storage unless explicitly configured
                        • You own and control all your data
                        • Backup is your responsibility
                    """.trimIndent()
                )

                // LLM Terms
                TermsSection(
                    title = "3. Local LLM Usage",
                    content = """
                        • Gemma 4B model runs entirely on your device
                        • No data sent to external AI services
                        • Model performance depends on device capabilities
                        • TTS (Text-to-Speech) runs locally via Piper
                        • STT (Speech-to-Text) requires device microphone
                    """.trimIndent()
                )

                // API Integration
                TermsSection(
                    title = "4. Third-Party Integrations",
                    content = """
                        • WhatsApp Business API requires your own API key
                        • Gmail integration for bank notifications (optional)
                        • You are responsible for API costs and compliance
                        • Integrations can be disabled at any time
                    """.trimIndent()
                )

                // No Liability
                TermsSection(
                    title = "5. Limitation of Liability",
                    content = """
                        • Q Manage is provided "as is" without warranties
                        • We are not liable for financial losses or data loss
                        • Regular backups are strongly recommended
                        • Critical financial decisions should involve professionals
                    """.trimIndent()
                )

                // User Responsibilities
                TermsSection(
                    title = "6. User Responsibilities",
                    content = """
                        • Maintain secure passwords and session tokens
                        • Keep the app and device software updated
                        • Review AI-generated insights before acting
                        • Comply with local tax and financial regulations
                        • Secure physical access to your device
                    """.trimIndent()
                )

                // Agreement checkbox
                Spacer(Modifier.height(8.dp))
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceVariant
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Checkbox(
                            checked = accepted,
                            onCheckedChange = { accepted = it }
                        )
                        Text(
                            "I have read and agree to the Terms & Conditions, Privacy Policy, and AI Disclaimer. I understand that Q Manage is a local-first ERP system and I am responsible for my data and financial decisions.",
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }

                Spacer(Modifier.height(80.dp)) // Space for buttons
            }

            // Bottom buttons
            Surface(
                tonalElevation = 3.dp,
                shadowElevation = 8.dp
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    OutlinedButton(
                        onClick = onDecline,
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("Decline")
                    }
                    Button(
                        onClick = onAccept,
                        modifier = Modifier.weight(1f),
                        enabled = accepted
                    ) {
                        Icon(Icons.Default.CheckCircle, null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Text("Accept & Continue")
                    }
                }
            }
        }
    }
}

@Composable
private fun TermsSection(title: String, content: String) {
    Card(
        modifier = Modifier.fillMaxWidth(),
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
            Text(
                title,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
                color = MaterialTheme.colorScheme.primary
            )
            Spacer(Modifier.height(8.dp))
            Text(
                content,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurface
            )
        }
    }
}
