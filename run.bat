@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo    Multithreaded OS Process Manager Simulator (1-Click Run)
echo ================================================================
echo.

:: 1. Check if gcc is already available in PATH
where gcc >nul 2>&1
if %errorlevel% equ 0 (
    goto :COMPILE
)

:: 2. Search common compiler paths on Windows (MSYS2, MinGW, UCRT64)
if exist "D:\msys\ucrt64\bin\gcc.exe" (
    set "PATH=D:\msys\ucrt64\bin;!PATH!"
    goto :COMPILE
)
if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set "PATH=C:\msys64\ucrt64\bin;!PATH!"
    goto :COMPILE
)
if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;!PATH!"
    goto :COMPILE
)
if exist "C:\MinGW\bin\gcc.exe" (
    set "PATH=C:\MinGW\bin;!PATH!"
    goto :COMPILE
)
if exist "D:\bin\gcc.exe" (
    set "PATH=D:\bin;!PATH!"
    goto :COMPILE
)

echo [ERROR] GCC compiler was not found in PATH or standard directories.
echo Please ensure MinGW-w64 or MSYS2 is installed.
echo.
pause
exit /b 1

:COMPILE
echo [*] Compiling project with GCC...
gcc -Wall -Wextra -pthread -std=c11 -Iinclude src/pm_sim.c src/process_manager.c -o pm_sim.exe
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Compilation failed.
    echo.
    pause
    exit /b %errorlevel%
)
echo [OK] Compilation successful: pm_sim.exe generated.
echo.

:RUN
if exist "snapshots.txt" (
    del /f /q "snapshots.txt"
    echo [*] Cleared previous snapshots.txt.
)

set "CHOICE=%~1"
if not "%CHOICE%"=="" goto :HANDLE_CHOICE

echo Select a simulation scenario to execute:
echo   [1] Standard Benchmark (2 Threads - Concurrent Worker Baseline)
echo   [2] Deep Hierarchy     (3 Threads - Multi-Level Process Tree)
echo   [3] High Concurrency   (3 Threads - Dual Waits ^& Multiple Exits)
echo   [4] Run All Scenarios  (Sequential Complete Suite)
echo.
set "CHOICE=1"
set /p "CHOICE=Enter choice (1-4) or press ENTER for default [1]: "
echo.

:HANDLE_CHOICE
if "%CHOICE%"=="1" goto :RUN_SCENARIO_1
if "%CHOICE%"=="2" goto :RUN_SCENARIO_2
if "%CHOICE%"=="3" goto :RUN_SCENARIO_3
if "%CHOICE%"=="4" goto :RUN_SCENARIO_4

echo [*] Running with custom arguments: %*
echo ----------------------------------------------------------------
pm_sim.exe %*
goto :POST_RUN

:RUN_SCENARIO_1
echo [*] Running Scenario 1: Standard Benchmark (tests\thread1.txt tests\thread2.txt)
echo ----------------------------------------------------------------
pm_sim.exe tests\thread1.txt tests\thread2.txt
goto :POST_RUN

:RUN_SCENARIO_2
echo [*] Running Scenario 2: Deep Process Tree (tests\tree_t0.txt tests\tree_t1.txt tests\tree_t2.txt)
echo ----------------------------------------------------------------
pm_sim.exe tests\tree_t0.txt tests\tree_t1.txt tests\tree_t2.txt
goto :POST_RUN

:RUN_SCENARIO_3
echo [*] Running Scenario 3: High Concurrency (tests\stress_t0.txt tests\stress_t1.txt tests\stress_t2.txt)
echo ----------------------------------------------------------------
pm_sim.exe tests\stress_t0.txt tests\stress_t1.txt tests\stress_t2.txt
goto :POST_RUN

:RUN_SCENARIO_4
echo ================================================================
echo [*] RUNNING SCENARIO 1 of 3: Standard Benchmark
echo ================================================================
pm_sim.exe tests\thread1.txt tests\thread2.txt
echo.
echo ================================================================
echo [*] RUNNING SCENARIO 2 of 3: Deep Process Tree
echo ================================================================
pm_sim.exe tests\tree_t0.txt tests\tree_t1.txt tests\tree_t2.txt
echo.
echo ================================================================
echo [*] RUNNING SCENARIO 3 of 3: High Concurrency
echo ================================================================
pm_sim.exe tests\stress_t0.txt tests\stress_t1.txt tests\stress_t2.txt
goto :POST_RUN

:POST_RUN
echo ----------------------------------------------------------------
echo.

if exist "snapshots.txt" (
    echo [*] Fresh process table snapshots captured in snapshots.txt:
    echo     (Showing first 18 lines)
    echo ----------------------------------------------------------------
    powershell -NoProfile -Command "Get-Content snapshots.txt -TotalCount 18"
    echo ... [See snapshots.txt for complete chronological history]
    echo ----------------------------------------------------------------
)

echo.
echo [DONE] Simulation finished successfully.
echo.

:: Pause only if double-clicked from Windows File Explorer
if not defined prompt (
    echo.
    pause
)
