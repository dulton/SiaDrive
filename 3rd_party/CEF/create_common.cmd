@echo off
set ROOT=%~dp0%
set PATH=%ROOT%..\..\bin;%PATH%
pushd "%ROOT%"
 
mkdir automate >NUL 2>&1
mkdir chromium_git >NUL 2>&1
mkdir depot_tools >NUL 2>&1

if not exist "automate\automate-git.py" (
  del /q automate-git.py > NUL
  wget --no-check-certificate https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py || goto :ERROR
  move /y automate-git.py automate\ || goto :ERROR
)

if not exist "depot_tools.complete" (
  del /q depot_tools.zip > NUL
  wget --no-check-certificate https://storage.googleapis.com/chrome-infra/depot_tools.zip || goto :ERROR
  unzip -o -q -d depot_tools\ depot_tools.zip || goto :ERROR
  del /q depot_tools.zip > NUL

  pushd depot_tools
    call update_depot_tools.bat
  popd
  
  echo "1">depot_tools.complete
)
goto :END

:ERROR
pause
popd
exit 1

:END
popd