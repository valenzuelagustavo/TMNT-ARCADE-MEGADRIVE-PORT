@echo off
rem ===========================================================================
rem build_rom_plain.bat (01/11) - compila la ROM SIN mapper (cabecera "SEGA MEGA DRIVE")
rem ===========================================================================
rem Para flash carts viejos / clones (Everdrive chino de hasta 4 MB, cartuchos
rem fisicos sin mapper). La ROM de la rama mapper mide menos de 4 MB, asi que no
rem necesita bank switch: sin ENABLE_BANK_SWITCH la cabecera sale como
rem "SEGA MEGA DRIVE" (fin de ROM 0x3FFFFF) y SGDK no toca los registros del
rem mapper al arrancar. Usa la libreria normal de SGDK (no lib_ssf).
rem
rem Hace un "clean" antes: los objetos de out\ del build con mapper no se pueden
rem mezclar con estos (y al reves, ver README.md de esta carpeta).
rem ===========================================================================
setlocal
if "%GDK_WIN%"=="" set "GDK_WIN=C:\SGDK"
set "PATH=%GDK_WIN%\bin;%PATH%"

pushd "%~dp0..\.."
make -f "%GDK_WIN%\makefile.gen" clean
make -f "%GDK_WIN%\makefile.gen" release
set ERR=%ERRORLEVEL%
if "%ERR%"=="0" python tools\mapper\check_mapper.py
popd
exit /b %ERR%
