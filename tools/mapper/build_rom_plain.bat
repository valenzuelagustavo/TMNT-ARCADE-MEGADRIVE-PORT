@echo off
rem ===========================================================================
rem build_rom_plain.bat (01/11) - compila la edicion LITE: ROM SIN mapper (hasta 4 MB)
rem ===========================================================================
rem Para flash carts viejos / clones (Everdrive MD v2.6 y anteriores, Everdrive
rem chino de hasta 4 MB, cartuchos fisicos sin mapper). Sin ENABLE_BANK_SWITCH
rem la cabecera sale "SEGA MEGA DRIVE" (fin de ROM 0x3FFFFF) y SGDK no toca los
rem registros del mapper al arrancar. Usa la libreria normal de SGDK (no lib_ssf).
rem Una sola base de codigo, dos ediciones: ver src\build_config.h (TMNT_LITE).
rem
rem Hace un "clean" antes: los objetos de out\ del build con mapper no se pueden
rem mezclar con estos. Al final check_edition.py comprueba la cabecera y que
rem entre en 4 MB, y deja la copia out\tmnt_megadrive_port_<version>_lite.bin.
rem ===========================================================================
setlocal
if "%GDK_WIN%"=="" set "GDK_WIN=C:\SGDK"
set "PATH=%GDK_WIN%\bin;%PATH%"

pushd "%~dp0..\.."
make -f "%GDK_WIN%\makefile.gen" clean
make -f "%GDK_WIN%\makefile.gen" release
set ERR=%ERRORLEVEL%
if "%ERR%"=="0" python tools\mapper\check_edition.py lite
if "%ERR%"=="0" set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
