@echo off
set ROOT=%~dp0%
set MODE=%1
set PATH=%ROOT%depot_tools;%PATH%
pushd "%ROOT%"

if not exist "%MODE%.complete" (
  setlocal
  call create_common.cmd
  endlocal
  
  set CEF_USE_GN=1
  if "%MODE%"=="Debug" (
    set GN_DEFINES=is_win_fastlink=true fatal_linker_warnings=false
  ) else (
    set GN_DEFINES=fatal_linker_warnings=false
  )
  set GN_ARGUMENTS=--ide=vs2015 --sln=cef --filters=//cef/*
  
  pushd chromium_git
    call python ..\automate\automate-git.py --download-dir=%ROOT%chromium_git --depot-tools-dir=%ROOT%depot_tools --no-distrib --no-build --branch=3029 || goto :ERROR
  popd
  
  pushd chromium_git\chromium\src\cef  
    call cef_create_projects.bat
  popd
  
  pushd chromium_git\chromium\src
    call ninja -C out\%MODE%_GN_x64 cef || goto :ERROR
  popd
  
  mkdir bin >NUL 2>&1
  mkdir bin\%MODE% >NUL 2>&1
  mkdir lib >NUL 2>&1
  mkdir lib\%MODE% >NUL 2>&1
  
  xcopy /s /y /i chromium_git\chromium\src\out\%MODE%_GN_x64\locales bin\%MODE%\locales\
  
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libcef.dll bin\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\chrome_elf.dll bin\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\osmesa.dll bin\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libEGL.dll bin\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libGLESv2.dll bin\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\d3dcompiler_47.dll bin\%MODE%\
  
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libcef.dll.lib lib\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\chrome_elf.dll.lib lib\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\osmesa.lib lib\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libEGL.dll.lib lib\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libGLESv2.dll.lib lib\%MODE%\ 
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\obj\cef\libcef_dll_wrapper.lib lib\%MODE%\ 

  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libcef.dll.pdb bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\chrome_elf.dll.pdb bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\osmesa.dll.pdb bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libEGL.dll.pdb bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libGLESv2.dll.pdb bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\obj\cef\libcef_dll_wrapper_cc.pdb bin\%MODE%\ 

  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libcef.dll.pdb lib\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\chrome_elf.dll.pdb lib\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\osmesa.dll.pdb lib\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libEGL.dll.pdb lib\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\libGLESv2.dll.pdb lib\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\obj\cef\libcef_dll_wrapper_cc.pdb lib\%MODE%\ 
    
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\icudtl.dat bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\cef.pak bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\*.bin bin\%MODE%\
  copy /y chromium_git\chromium\src\out\%MODE%_GN_x64\cef_*.pak bin\%MODE%\
  
  echo "Cleaning build output... Please wait..."
  rd /s /q chromium_git\chromium\src\out\%MODE%_GN_x64
  rd /s /q chromium_git\chromium\src\out\%MODE%_GN_x86
  
  echo "1">%MODE%.complete
)
  
goto :END

:ERROR
  pause
  popd
  exit 1
  
:END
  popd