@echo off
rem ===========================================================================
rem build_rom_ssf.bat (23/10) - compila la ROM CON mapper SSF (bank switch)
rem ===========================================================================
rem Desde cualquier carpeta: compila el proyecto con ENABLE_BANK_SWITCH=1 y la
rem libreria de C:\SGDK\lib_ssf (ver build_sgdk_lib_ssf.bat) y despues revisa
rem el reparto de la ROM con check_mapper.py.
rem
rem Hace un "clean" antes: los objetos de out\ compilados sin el flag (la rama
rem main, Genesis Code) no se pueden mezclar con estos.
rem ===========================================================================
setlocal
if "%GDK_WIN%"=="" set "GDK_WIN=C:\SGDK"
set "GDK=%GDK_WIN:\=/%"
set "PATH=%GDK_WIN%\bin;%PATH%"

if not exist "%GDK_WIN%\lib_ssf\libmd.a" (
    echo Falta %GDK_WIN%\lib_ssf\libmd.a: correr primero tools\mapper\build_sgdk_lib_ssf.bat
    exit /b 1
)

pushd "%~dp0..\.."
make -f "%GDK_WIN%\makefile.gen" clean
make -f "%GDK_WIN%\makefile.gen" "EXTRA_FLAGS=-DENABLE_BANK_SWITCH=1" "LIBMD=%GDK%/lib_ssf/libmd.a" release
set ERR=%ERRORLEVEL%
if "%ERR%"=="0" python tools\mapper\check_mapper.py
if "%ERR%"=="0" set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
