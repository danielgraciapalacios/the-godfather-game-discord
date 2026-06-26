@echo off
REM Compila la version basica (x86) con MSVC + CMake (generador Ninja embebido en VS).
REM Uso: build.cmd            -> compila Release (la .asi)
REM      build.cmd test       -> compila y ejecuta unit_tests

setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
set "ROOT=%~dp0"

call "%VCVARS%" || (echo [ERROR] No se encontro vcvars32.bat & exit /b 1)

"%CMAKE%" -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1

if /I "%1"=="test" (
  "%CMAKE%" --build "%ROOT%build" --target unit_tests || exit /b 1
  "%ROOT%build\unit_tests.exe" || exit /b 1
  goto :eof
)

"%CMAKE%" --build "%ROOT%build" || exit /b 1
echo.
echo [OK] Binario generado en: %ROOT%build\GodfatherDiscordRPC.asi
