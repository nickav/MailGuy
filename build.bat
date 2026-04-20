@echo off

set project_root=%~dp0%

rem set flags=/Od -Oi -Zo -Z7 -FC -Gm- -GR- /Zc:strictStrings-
set flags=/O2 -Oi -Zo -Z7 -FC -Gm- -GR- /Zc:strictStrings-
set exe=MailGuy.exe

pushd %project_root%

if not exist build (mkdir build)
pushd build

    ntime.exe cl -nologo %flags% -DDEBUG=1 /I ..\src ..\src\win32_main.c /link /subsystem:windows -incremental:no -opt:ref -OUT:%exe%
    IF %errorlevel% NEQ 0 (goto end)
    
    .\%exe%

:end
popd

popd
exit /B %errorlevel%