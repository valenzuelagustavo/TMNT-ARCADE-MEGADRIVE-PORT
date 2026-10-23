@echo off
rem ===========================================================================
rem build_sgdk_lib_ssf.bat (23/10) - libreria de SGDK CON mapper SSF
rem ===========================================================================
rem Compila la libreria de SGDK con ENABLE_BANK_SWITCH=1 en %GDK%\lib_ssf\
rem (C:\SGDK\lib_ssf\libmd.a). La libreria normal (C:\SGDK\lib\libmd.a) NO se
rem toca: la rama main sigue compilando igual que siempre.
rem
rem Hace falta correrlo UNA vez (y de nuevo si se actualiza SGDK). Necesita
rem que C:\SGDK\inc\config.h acepte el flag desde afuera (#ifndef
rem ENABLE_BANK_SWITCH), ver tools\mapper\README.md.
rem
rem lib_ssf va adentro de SGDK porque make no soporta rutas con espacios
rem ("G:\TMNT MEGADRIVE\...").
rem ===========================================================================
setlocal
if "%GDK_WIN%"=="" set "GDK_WIN=C:\SGDK"
set "GDK=%GDK_WIN:\=/%"
set "PATH=%GDK_WIN%\bin;%PATH%"

if not exist "%GDK_WIN%\lib_ssf" mkdir "%GDK_WIN%\lib_ssf"

pushd "%GDK_WIN%"
rem Objetos de la libreria limpios antes y despues: asi la proxima vez que se
rem compile la libreria normal no reusa objetos con el mapper.
make -f "%GDK_WIN%\makelib.gen" cleanobj cleandep
make -f "%GDK_WIN%\makelib.gen" "LIB=%GDK%/lib_ssf" "EXTRA_FLAGS=-DENABLE_BANK_SWITCH=1" release
set ERR=%ERRORLEVEL%
make -f "%GDK_WIN%\makelib.gen" cleanobj cleandep
popd

if not "%ERR%"=="0" (
    echo.
    echo ERROR compilando la libreria con mapper.
    exit /b %ERR%
)
echo.
echo Listo: %GDK_WIN%\lib_ssf\libmd.a
endlocal
