@echo off
setlocal
set "VSROOT=C:\Program Files\Microsoft Visual Studio\18\Community"
set "CMAKE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%CMAKE%" (
    echo [CP2077Trainer] cmake.exe not found: %CMAKE%
    exit /b 1
)
if not exist "%VCVARS%" (
    echo [CP2077Trainer] vcvars64.bat not found: %VCVARS%
    exit /b 1
)

call "%VCVARS%" >nul
cd /d "%~dp0"

"%CMAKE%" -S . -B build -G "Visual Studio 18 2026" -A x64
if errorlevel 1 exit /b 1

"%CMAKE%" --build build --config Release
if errorlevel 1 exit /b 1

echo.
echo [CP2077Trainer] OK: build\bin\Release\CP2077Trainer.exe
endlocal
