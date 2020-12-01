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

set DEVTOOL_OPTIONS=-p=UART_driver.FullSysIdeProj -AutoBuild -Autorun -IAcceptLicense -NoEnvFile -Minimize -lf5=Sim.Log -q -ws=217

echo .
echo DevTool:         %DEVTOOL%
echo DEVTOOL_OPTIONS: %DEVTOOL_OPTIONS%
echo CmdLineArgs:     %1  %2  %3  %4
echo .


echo ============================================================
echo Running UART system test
echo ============================================================
del Sim.log
%DEVTOOL% %DEVTOOL_OPTIONS% %1 %2 %3 %4
if  %ERRORLEVEL% NEQ 0 ( goto errors )


echo .
echo All UART System Tests Pass


rem Run Stand-Alone eTPU Tests

pushd test
call  Test.bat  %1  %2
popd
if  %ERRORLEVEL% NEQ 0 ( goto errors )


goto end
:errors
echo *************************************************
echo        YIKES, WE GOT ERRORS!!
echo *************************************************
exit /b -1
:end
exit /b 0
