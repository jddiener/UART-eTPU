setlocal
echo off

rem During installation of the ASH WARE 
rem environment variable, 'DEV_TOOL_FULL_SYS_BIN', is set to point
rem to the latest installed version
set DEVTOOL=%DEV_TOOL_FULL_SYS_BIN%\FullSystemDevTool.exe
if exist %DEVTOOL% goto FOUND_DEVTOOL

echo .
echo *************************************************
echo It appears the  'DEV_TOOL_FULL_SYS_BIN'  environment variable 
echo is not set, or not set correctly.
echo DEV_TOOL_FULL_SYS_BIN = %DEV_TOOL_FULL_SYS_BIN%
echo Correct this problem in order to build this demo (install System DevTool).
echo *************************************************
pause
echo .
goto errors

:FOUND_DEVTOOL

set DEVTOOL_OPTIONS=-AutoRun -NoEnvFile -Minimize -q -lf5=Sim.log
set DEVTOOL_OPTIONS_BUILD_ONLY=-AutoBuild -NoEnvFile -Minimize -q -lf5=Sim.log

echo .
echo DevTool:         %DEVTOOL%
echo DEVTOOL_OPTIONS: %DEVTOOL_OPTIONS%
echo CmdLineArgs:     %1  %2  %3  %4
echo .

echo Build code ...
%DEVTOOL% -p=Proj.ETpuIdeProj %DEVTOOL_OPTIONS_BUILD_ONLY% %1 %2 %3 %4
if  %ERRORLEVEL% NEQ 0 ( goto errors )

echo -------------------------------------------
echo Running UART Single-Target Tests ...

echo Deleting coverage data, etc ...
del  *.CoverageData *.report *.log  /Q

echo Running "BasicMode" Test ...
%DEVTOOL% -p=Proj.ETpuIdeProj -s=BasicMode.ETpuCommand -NoBuild %DEVTOOL_OPTIONS% %1 %2 %3 %4
if  %ERRORLEVEL% NEQ 0 ( goto errors )

echo Running "FlowControl" Test ...
%DEVTOOL% -p=Proj.ETpuIdeProj -s=FlowControl.ETpuCommand -NoBuild %DEVTOOL_OPTIONS% %1 %2 %3 %4
if  %ERRORLEVEL% NEQ 0 ( goto errors )

echo Running "FrameError" Test ...
%DEVTOOL% -p=Proj.ETpuIdeProj -s=FrameError.ETpuCommand -NoBuild %DEVTOOL_OPTIONS% %1 %2 %3 %4
if  %ERRORLEVEL% NEQ 0 ( goto errors )

echo Running "Parity" Test ...
%DEVTOOL% -p=Proj.ETpuIdeProj -s=Parity.ETpuCommand -NoBuild %DEVTOOL_OPTIONS% %1 %2 %3 %4
if  %ERRORLEVEL% NEQ 0 ( goto errors )

echo .
echo All UART Single-Target Tests Pass

goto end
:errors
echo *************************************************
echo        YIKES, WE GOT ERRORS!!
echo *************************************************
exit /b -1
:end
exit /b 0
