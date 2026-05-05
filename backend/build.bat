@echo off
echo ============================================
echo   Q Manage — Edge-Native ERP Build System
echo   Zoho Books + CRM + Analytics Killer
echo ============================================

set CC=gcc
set CFLAGS=-O3 -Wall -Wno-unused-result -I./include -I./deps -D_FILE_OFFSET_BITS=64 -Dfseeko=fseek -Dftello=ftell
set LDFLAGS=-lws2_32 -liphlpapi

set SOURCES=^
  src/main.c ^
  src/api.c ^
  src/db.c ^
  src/ledger.c ^
  src/crypto.c ^
  src/audit.c ^
  src/auth.c ^
  src/backup.c ^
  src/banking.c ^
  src/migration.c ^
  src/erp.c ^
  src/reports_engine.c ^
  src/gst.c ^
  src/gmail_bot.c ^
  src/gdrive.c ^
  deps/mongoose.c ^
  deps/cJSON.c ^
  deps/sqlite3.c

echo Compiling %SOURCES%...
%CC% %CFLAGS% %SOURCES% -o qmanage_server.exe %LDFLAGS%

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed! Make sure gcc is in your PATH.
    exit /b %errorlevel%
)

echo.
echo [SUCCESS] Build complete! 
echo Run: qmanage_server.exe
echo Server: http://localhost:9741
