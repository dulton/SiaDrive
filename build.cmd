@echo off
set ROOT=%~dp0%
set TARGET_MODE=%1
pushd "%ROOT%"

set CMAKE=%ROOT%bin\cmake-3.7.2-win64-x64\bin\cmake

setlocal
call build_common.cmd
endlocal

setlocal
call 3rd_party\CEF\create.cmd %TARGET_MODE%
endlocal

mkdir build >NUL 2>&1
mkdir build\%TARGET_MODE% >NUL 2>&1
pushd build\%TARGET_MODE% >NUL 2>&1
  (%CMAKE% -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE=%TARGET_MODE% -DSIADRIVE_INSTALL_FOLDER="%ROOT%dist\%TARGET_MODE%" ..\..) && (
    %CMAKE% --build . --config %TARGET_MODE%) && (
    %CMAKE% --build . --target install --config %TARGET_MODE% && (
      rd /s /q "%ROOT%dist\%TARGET_MODE%\config"
      rd /s /q "%ROOT%dist\%TARGET_MODE%\htdocs\.idea"
      del /q "%ROOT%dist\%TARGET_MODE%\*.log"
      rd /s /q "%ROOT%dist\%TARGET_MODE%\logs"
      if "%TARGET_MODE%"=="Release" (
        del /q "%ROOT%dist\%TARGET_MODE%\*.lib"
        del /q "%ROOT%dist\%TARGET_MODE%\*.pdb"
      )
    )
  )
popd

popd