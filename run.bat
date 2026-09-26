@echo off
if exist "build\Release\bedrock-dumper.exe" (
    "build\Release\bedrock-dumper.exe"
) else (
    echo [!] bedrock-dumper.exe not found. Please build the project first using CMake.
)
pause
