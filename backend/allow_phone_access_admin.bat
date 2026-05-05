@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Run this script as Administrator.
    exit /b 1
)

netsh advfirewall firewall add rule name="QManage 9741" dir=in action=allow protocol=TCP localport=9741 profile=any
if %errorlevel% neq 0 (
    echo Failed to add firewall rule.
    exit /b %errorlevel%
)

echo Firewall rule added for TCP 9741.
echo You can now test: http://^<your-lan-ip^>:9741/