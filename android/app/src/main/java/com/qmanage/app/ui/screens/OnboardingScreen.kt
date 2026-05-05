@file:OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class, ExperimentalAnimationApi::class, ExperimentalLayoutApi::class)
package com.qmanage.app.ui.screens

import androidx.compose.animation.*
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

// Onboarding Steps
sealed class OnboardingStep(val title: String, val icon: androidx.compose.ui.graphics.vector.ImageVector) {
    object Language : OnboardingStep("Language", Icons.Default.Language)
    object CSVImport : OnboardingStep("Import Data", Icons.Default.UploadFile)
    object GmailSetup : OnboardingStep("Gmail Setup", Icons.Default.Email)
    object LLMSetup : OnboardingStep("AI Setup", Icons.Default.Psychology)
    object TTSConfig : OnboardingStep("Voice Setup", Icons.Default.Mic)
    object Terms : OnboardingStep("Terms", Icons.Default.Gavel)
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalAnimationApi::class)
@Composable
fun OnboardingScreen(
    onComplete: () -> Unit,
    onSkip: () -> Unit
) {
    val steps = listOf(
        OnboardingStep.Language,
        OnboardingStep.CSVImport,
        OnboardingStep.GmailSetup,
        OnboardingStep.LLMSetup,
        OnboardingStep.TTSConfig,
        OnboardingStep.Terms
    )
    
    var currentStepIndex by remember { mutableStateOf(0) }
    var canProceed by remember { mutableStateOf(false) }
    
    val currentStep = steps[currentStepIndex]
    val progress = (currentStepIndex + 1).toFloat() / steps.size

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Setup", fontWeight = FontWeight.SemiBold) },
                actions = {
                    TextButton(onClick = onSkip) {
                        Text("Skip All")
                    }
                },
                navigationIcon = {
                    if (currentStepIndex > 0) {
                        IconButton(onClick = { currentStepIndex-- }) {
                            Icon(Icons.Default.ArrowBack, "Back")
                        }
                    }
                }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .fillMaxSize()
        ) {
            // Progress indicator
            LinearProgressIndicator(
                progress = { progress },
                modifier = Modifier.fillMaxWidth(),
            )
            
            // Step indicators - horizontally scrollable
            androidx.compose.foundation.lazy.LazyRow(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                items(steps.size) { index ->
                    val step = steps[index]
                    val isCompleted = index < currentStepIndex
                    val isCurrent = index == currentStepIndex
                    
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Surface(
                            shape = MaterialTheme.shapes.small,
                            color = when {
                                isCompleted -> MaterialTheme.colorScheme.primary
                                isCurrent -> MaterialTheme.colorScheme.primaryContainer
                                else -> MaterialTheme.colorScheme.surfaceVariant
                            },
                            modifier = Modifier.size(36.dp)
                        ) {
                            Box(contentAlignment = Alignment.Center) {
                                if (isCompleted) {
                                    Icon(
                                        Icons.Default.Check,
                                        contentDescription = null,
                                        tint = MaterialTheme.colorScheme.onPrimary,
                                        modifier = Modifier.size(20.dp)
                                    )
                                } else {
                                    Icon(
                                        step.icon,
                                        contentDescription = step.title,
                                        tint = if (isCurrent) 
                                            MaterialTheme.colorScheme.onPrimaryContainer 
                                        else 
                                            MaterialTheme.colorScheme.onSurfaceVariant,
                                        modifier = Modifier.size(20.dp)
                                    )
                                }
                            }
                        }
                        Text(
                            step.title,
                            style = MaterialTheme.typography.labelSmall,
                            color = if (isCurrent) 
                                MaterialTheme.colorScheme.primary 
                            else 
                                MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1
                        )
                    }
                }
            }

            // Step content
            AnimatedContent(
                targetState = currentStep,
                transitionSpec = {
                    (slideInHorizontally { fullWidth -> fullWidth } + fadeIn()) togetherWith
                    (slideOutHorizontally { fullWidth -> -fullWidth } + fadeOut())
                },
                modifier = Modifier.weight(1f)
            ) { step ->
                when (step) {
                    OnboardingStep.Language -> LanguageStep(
                        onSelected = { canProceed = true }
                    )
                    OnboardingStep.CSVImport -> CSVImportStep(
                        onImported = { canProceed = true }
                    )
                    OnboardingStep.GmailSetup -> GmailSetupStep(
                        onConfigured = { canProceed = true }
                    )
                    OnboardingStep.LLMSetup -> LLMSetupStep(
                        onConfigured = { canProceed = true }
                    )
                    OnboardingStep.TTSConfig -> TTSConfigStep(
                        onConfigured = { canProceed = true }
                    )
                    OnboardingStep.Terms -> TermsAcceptanceStep(
                        onAccepted = { canProceed = true }
                    )
                }
            }

            // Navigation buttons
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
                    if (currentStepIndex < steps.size - 1) {
                        OutlinedButton(
                            onClick = { currentStepIndex++ },
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Skip")
                        }
                        Button(
                            onClick = { currentStepIndex++ },
                            modifier = Modifier.weight(1f),
                            enabled = canProceed || currentStep == OnboardingStep.Language
                        ) {
                            Text("Next")
                            Spacer(Modifier.width(4.dp))
                            Icon(Icons.Default.ArrowForward, null, modifier = Modifier.size(18.dp))
                        }
                    } else {
                        Button(
                            onClick = onComplete,
                            modifier = Modifier.fillMaxWidth(),
                            enabled = canProceed
                        ) {
                            Icon(Icons.Default.CheckCircle, null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(8.dp))
                            Text("Complete Setup")
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun LanguageStep(onSelected: () -> Unit) {
    var selectedLang by remember { mutableStateOf<String?>(null) }
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "Select Your Language",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    "This will be used for the app interface and AI assistant responses",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        // Quick language selection
        val quickLanguages = listOf(
            "en" to "English 🇬🇧",
            "hi" to "Hindi 🇮🇳",
            "ta" to "Tamil 🇮🇳",
            "te" to "Telugu 🇮🇳",
            "mr" to "Marathi 🇮🇳",
            "gu" to "Gujarati 🇮🇳"
        )

        quickLanguages.forEach { (code, name) ->
            Card(
                modifier = Modifier.fillMaxWidth(),
                onClick = {
                    selectedLang = code
                    onSelected()
                },
                colors = CardDefaults.cardColors(
                    containerColor = if (selectedLang == code)
                        MaterialTheme.colorScheme.primaryContainer
                    else
                        MaterialTheme.colorScheme.surface
                ),
                border = androidx.compose.foundation.BorderStroke(
                    width = if (selectedLang == code) 2.dp else 1.dp,
                    color = if (selectedLang == code)
                        MaterialTheme.colorScheme.primary
                    else
                        MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)
                )
            ) {
                Text(
                    name,
                    modifier = Modifier.padding(16.dp),
                    style = MaterialTheme.typography.bodyLarge
                )
            }
        }
    }
}

@Composable
private fun CSVImportStep(onImported: () -> Unit) {
    var hasImported by remember { mutableStateOf(false) }
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "Import from Zoho Books",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    "Upload your CSV export from Zoho Books/CRM",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        Card(
            modifier = Modifier.fillMaxWidth(),
            border = androidx.compose.foundation.BorderStroke(
                width = 2.dp,
                color = MaterialTheme.colorScheme.primary.copy(alpha = 0.5f)
            )
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(32.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Icon(
                    Icons.Default.UploadFile,
                    contentDescription = null,
                    modifier = Modifier.size(64.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(Modifier.height(16.dp))
                Text(
                    "Tap to upload CSV file",
                    style = MaterialTheme.typography.titleMedium
                )
                Text(
                    "Supports: contacts.csv, invoices.csv, items.csv",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(Modifier.height(16.dp))
                Button(
                    onClick = { hasImported = true; onImported() }
                ) {
                    Text("Select File")
                }
            }
        }

        if (hasImported) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.tertiaryContainer
                )
            ) {
                Row(
                    modifier = Modifier.padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Icon(
                        Icons.Default.CheckCircle,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onTertiaryContainer
                    )
                    Text(
                        "Data imported successfully!",
                        color = MaterialTheme.colorScheme.onTertiaryContainer
                    )
                }
            }
        }

        OutlinedButton(
            onClick = { hasImported = true; onImported() },
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Skip Import (Start Fresh)")
        }
    }
}

@Composable
private fun GmailSetupStep(onConfigured: () -> Unit) {
    var email by remember { mutableStateOf("") }
    var isConnected by remember { mutableStateOf(false) }
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "Bank Notifications via Gmail",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    "Connect your Gmail to automatically track bank transactions",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        if (!isConnected) {
            OutlinedTextField(
                value = email,
                onValueChange = { email = it },
                label = { Text("Gmail Address") },
                placeholder = { Text("your.email@gmail.com") },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                leadingIcon = { Icon(Icons.Default.AlternateEmail, null) }
            )

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                )
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        "How it works:",
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold
                    )
                    Spacer(Modifier.height(8.dp))
                    Text("• We'll scan for bank notification emails")
                    Text("• Extract transaction details automatically")
                    Text("• All processing happens on your device")
                    Text("• You can revoke access anytime")
                }
            }

            Button(
                onClick = { isConnected = true; onConfigured() },
                modifier = Modifier.fillMaxWidth(),
                enabled = email.contains("@")
            ) {
                Icon(Icons.Default.Link, null)
                Spacer(Modifier.width(8.dp))
                Text("Connect Gmail")
            }
        } else {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.tertiaryContainer
                )
            ) {
                Row(
                    modifier = Modifier.padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Icon(
                        Icons.Default.CheckCircle,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onTertiaryContainer
                    )
                    Column {
                        Text(
                            "Gmail Connected",
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onTertiaryContainer
                        )
                        Text(
                            email,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onTertiaryContainer
                        )
                    }
                }
            }
        }

        OutlinedButton(
            onClick = { isConnected = true; onConfigured() },
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Skip Gmail Setup")
        }
    }
}

@Composable
private fun LLMSetupStep(onConfigured: () -> Unit) {
    var selectedModel by remember { mutableStateOf("gemma3:4b") }
    var isDownloaded by remember { mutableStateOf(false) }
    var downloadProgress by remember { mutableStateOf(0f) }
    
    val models = listOf(
        Triple("gemma3:4b", "Gemma 3 4B (Recommended)", "~2.5GB • Balanced performance"),
        Triple("gemma3:1b", "Gemma 3 1B (Lite)", "~700MB • Faster, less capable"),
        Triple("phi4", "Phi-4 Mini", "~2GB • Microsoft model"),
        Triple("qwen2.5", "Qwen 2.5 3B", "~1.8GB • Multilingual"),
        Triple("llama3.2", "Llama 3.2 3B", "~2GB • Meta model")
    )
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "AI Assistant Setup",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    "Choose a local LLM that runs entirely on your device",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        if (!isDownloaded) {
            Text(
                "Select Model:",
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold
            )

            models.forEach { (id, name, desc) ->
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    onClick = { selectedModel = id },
                    colors = CardDefaults.cardColors(
                        containerColor = if (selectedModel == id)
                            MaterialTheme.colorScheme.primaryContainer
                        else
                            MaterialTheme.colorScheme.surface
                    ),
                    border = androidx.compose.foundation.BorderStroke(
                        width = if (selectedModel == id) 2.dp else 1.dp,
                        color = if (selectedModel == id)
                            MaterialTheme.colorScheme.primary
                        else
                            MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        horizontalArrangement = Arrangement.spacedBy(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        RadioButton(
                            selected = selectedModel == id,
                            onClick = { selectedModel = id }
                        )
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                name,
                                fontWeight = if (selectedModel == id) 
                                    FontWeight.SemiBold 
                                else 
                                    FontWeight.Normal
                            )
                            Text(
                                desc,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }

            Spacer(Modifier.height(8.dp))

            if (downloadProgress > 0 && downloadProgress < 1) {
                LinearProgressIndicator(
                    progress = { downloadProgress },
                    modifier = Modifier.fillMaxWidth()
                )
                Text(
                    "Downloading... ${(downloadProgress * 100).toInt()}%",
                    style = MaterialTheme.typography.bodySmall
                )
            } else {
                Button(
                    onClick = { 
                        // Simulate download
                        isDownloaded = true
                        onConfigured()
                    },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Icon(Icons.Default.Download, null)
                    Spacer(Modifier.width(8.dp))
                    Text("Download & Install")
                }
            }
        } else {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.tertiaryContainer
                )
            ) {
                Row(
                    modifier = Modifier.padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Icon(
                        Icons.Default.CheckCircle,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onTertiaryContainer
                    )
                    Column {
                        Text(
                            "AI Model Ready",
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onTertiaryContainer
                        )
                        Text(
                            selectedModel,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onTertiaryContainer
                        )
                    }
                }
            }
        }

        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surfaceVariant
            )
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "Important Notes:",
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.SemiBold
                )
                Spacer(Modifier.height(8.dp))
                Text("• Model runs entirely offline on your device")
                Text("• Requires ~2-3GB storage space")
                Text("• Performance depends on device capabilities")
                Text("• Can be changed later in Settings")
            }
        }
    }
}

@Composable
private fun TTSConfigStep(onConfigured: () -> Unit) {
    var selectedTTS by remember { mutableStateOf("piper") }
    var selectedLanguages by remember { mutableStateOf(setOf("en")) }
    
    val ttsEngines = listOf(
        Triple("piper", "Piper TTS", "Fast, lightweight, high quality"),
        Triple("espeak", "eSpeak NG", "Lightweight, robotic voice"),
        Triple("system", "System TTS", "Use device default TTS")
    )
    
    val ttsLanguages = listOf(
        "en" to "English",
        "hi" to "Hindi",
        "ta" to "Tamil",
        "te" to "Telugu",
        "mr" to "Marathi"
    )
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        )
        {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "Text-to-Speech Setup",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    "Configure voice output for the AI assistant",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        Text(
            "TTS Engine:",
            style = MaterialTheme.typography.titleSmall,
            fontWeight = FontWeight.SemiBold
        )

        ttsEngines.forEach { (id, name, desc) ->
            Card(
                modifier = Modifier.fillMaxWidth(),
                onClick = { selectedTTS = id },
                colors = CardDefaults.cardColors(
                    containerColor = if (selectedTTS == id)
                        MaterialTheme.colorScheme.primaryContainer
                    else
                        MaterialTheme.colorScheme.surface
                ),
                border = androidx.compose.foundation.BorderStroke(
                    width = if (selectedTTS == id) 2.dp else 1.dp,
                    color = if (selectedTTS == id)
                        MaterialTheme.colorScheme.primary
                    else
                        MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)
                )
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = selectedTTS == id,
                        onClick = { selectedTTS = id }
                    )
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            name,
                            fontWeight = if (selectedTTS == id) 
                                FontWeight.SemiBold 
                            else 
                                FontWeight.Normal
                        )
                        Text(
                            desc,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }
        }

        Spacer(Modifier.height(8.dp))

        Text(
            "Preferred Languages (select 1-3):",
            style = MaterialTheme.typography.titleSmall,
            fontWeight = FontWeight.SemiBold
        )

        FlowRow(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            ttsLanguages.forEach { (code, name) ->
                val isSelected = code in selectedLanguages
                FilterChip(
                    selected = isSelected,
                    onClick = {
                        selectedLanguages = if (isSelected) {
                            selectedLanguages - code
                        } else if (selectedLanguages.size < 3) {
                            selectedLanguages + code
                        } else {
                            selectedLanguages
                        }
                    },
                    label = { Text(name) },
                    leadingIcon = if (isSelected) {
                        {
                            Icon(
                                Icons.Default.Check,
                                contentDescription = null,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                    } else null
                )
            }
        }

        Spacer(Modifier.weight(1f))

        Button(
            onClick = { onConfigured() },
            modifier = Modifier.fillMaxWidth(),
            enabled = selectedLanguages.isNotEmpty()
        ) {
            Icon(Icons.Default.Check, null)
            Spacer(Modifier.width(8.dp))
            Text("Confirm TTS Settings")
        }
    }
}

@Composable
private fun TermsAcceptanceStep(onAccepted: () -> Unit) {
    var accepted by remember { mutableStateOf(false) }
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(
                    "Final Step: Terms & Conditions",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    "Review and accept to complete setup",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        Card(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f),
            border = androidx.compose.foundation.BorderStroke(
                width = 1.dp,
                color = MaterialTheme.colorScheme.outlineVariant
            )
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(16.dp)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Text(
                    "Q Manage - Edge-Native ERP",
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.SemiBold
                )
                
                Text(
                    "By using this app, you agree to:",
                    style = MaterialTheme.typography.bodyMedium
                )
                
                Text("• AI responses are for informational purposes only")
                Text("• Financial decisions should be verified independently")
                Text("• All data is stored locally on your device")
                Text("• You are responsible for data backups")
                Text("• Third-party API usage (WhatsApp, Gmail) is your responsibility")
                
                Spacer(Modifier.height(8.dp))
                
                Text(
                    "Privacy:",
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.SemiBold
                )
                Text("• No data sent to external servers without permission")
                Text("• Local SQLite with AES-256-GCM encryption")
                Text("• You own and control all your data")
            }
        }

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
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Checkbox(
                    checked = accepted,
                    onCheckedChange = { 
                        accepted = it
                        if (it) onAccepted()
                    }
                )
                Text(
                    "I have read and agree to the Terms & Conditions, Privacy Policy, and AI Disclaimer",
                    style = MaterialTheme.typography.bodyMedium
                )
            }
        }
    }
}
