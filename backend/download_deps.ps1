# PowerShell script to download C dependencies for Q Manage Backend

$DepsDir = "C:\Users\Vicky\.gemini\antigravity\scratch\q-manage\backend\deps"
if (-not (Test-Path $DepsDir)) {
    New-Item -ItemType Directory -Force -Path $DepsDir
}

Write-Host "Downloading Mongoose (Web Server)..."
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/cesanta/mongoose/master/mongoose.c" -OutFile "$DepsDir\mongoose.c"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/cesanta/mongoose/master/mongoose.h" -OutFile "$DepsDir\mongoose.h"

Write-Host "Downloading cJSON (JSON Parser)..."
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c" -OutFile "$DepsDir\cJSON.c"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h" -OutFile "$DepsDir\cJSON.h"

Write-Host "Downloading SQLite (Database)..."
# We fetch the amalgamation zip and extract it
$SqliteUrl = "https://sqlite.org/2024/sqlite-amalgamation-3450300.zip"
$SqliteZip = "$DepsDir\sqlite.zip"
Invoke-WebRequest -Uri $SqliteUrl -OutFile $SqliteZip
Expand-Archive -Path $SqliteZip -DestinationPath $DepsDir -Force
Move-Item -Path "$DepsDir\sqlite-amalgamation-3450300\sqlite3.c" -Destination "$DepsDir\sqlite3.c" -Force
Move-Item -Path "$DepsDir\sqlite-amalgamation-3450300\sqlite3.h" -Destination "$DepsDir\sqlite3.h" -Force
Remove-Item -Path $SqliteZip
Remove-Item -Path "$DepsDir\sqlite-amalgamation-3450300" -Recurse -Force

Write-Host "All dependencies downloaded successfully!"
