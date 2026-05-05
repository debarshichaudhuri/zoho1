@echo off
cd /d "%~dp0"

if not exist qmanage_server.exe (
    echo qmanage_server.exe not found. Build the backend first with build.bat.
    exit /b 1
)

echo Starting Q Manage on http://0.0.0.0:9741
echo Open http://^<your-lan-ip^>:9741/ from another device on your network.
qmanage_server.exe