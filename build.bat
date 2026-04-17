@echo off

set project_root=%~dp0%

set flags=/O2 -Oi -Zo -Z7 -FC -Gm- -GR- /Zc:strictStrings-
rem set libs=%project_root%/prebuilt/curl/lib/libcurl.lib
rem set inc=/I %project_root%/prebuilt/curl/include
set exe=main.exe

pushd %project_root%

if not exist build (mkdir build)
pushd build

    ntime.exe cl -nologo %flags% -DDEBUG=1 /I ..\src ..\src\win32_main.c %inc% /link %libs% /subsystem:windows -incremental:no -opt:ref -OUT:%exe%
    IF %errorlevel% NEQ 0 (goto end)
    
    .\%exe%

:end
popd

popd
exit /B %errorlevel%