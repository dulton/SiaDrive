@echo off
set ROOT=%~dp0%
set PATH=%ROOT%bin;%PATH%
pushd "%ROOT%"
set DOKANY_VERSION=1.0.3

if NOT EXIST "3rd_party\dokany-%DOKANY_VERSION%.complete" (
  del /q v%DOKANY_VERSION%.zip > NUL
  wget --no-check-certificate "https://github.com/dokan-dev/dokany/archive/v%DOKANY_VERSION%.zip" || goto :ERROR
  unzip -o -q -d 3rd_party v%DOKANY_VERSION%.zip || goto :ERROR
  del /q v%DOKANY_VERSION%.zip > NUL

  pushd "3rd_party\dokany-%DOKANY_VERSION%"
    copy /y ..\enable_debug_logging.patch
    patch -p1 -N < enable_debug_logging.patch || goto :ERROR
    call build.bat
  popd   
  echo "1">3rd_party\dokany-%DOKANY_VERSION%.complete
)

if NOT EXIST ".\bin\cmake-3.7.2-win64-x64\bin\cmake.exe" (
  del /q cmake-3.7.2-win64-x64.zip > NUL
  wget --no-check-certificate https://cmake.org/files/v3.7/cmake-3.7.2-win64-x64.zip || goto :ERROR
  unzip -o -q -d bin\ cmake-3.7.2-win64-x64.zip || goto :ERROR
  del /q cmake-3.7.2-win64-x64.zip > NUL
)

goto :END

:ERROR
pause
popd
exit 1

:END
popd