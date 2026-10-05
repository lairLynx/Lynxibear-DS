@echo off
setlocal

if not exist "C:\msys64\usr\bin\bash.exe" (
    echo MSYS2 was not found at C:\msys64.
    exit /b 1
)

pushd "%~dp0"
set "PATH=C:\msys64\usr\bin;C:\msys64\opt\wonderful\bin;C:\msys64\opt\wonderful\toolchain\gcc-arm-none-eabi\bin;%PATH%"
"C:\msys64\usr\bin\bash.exe" -c "source /opt/wonderful/bin/wf-env -a && make %*"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
