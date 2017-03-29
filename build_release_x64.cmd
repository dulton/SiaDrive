@echo off
pushd "%~dp0%"
call build.cmd Release
pause
popd