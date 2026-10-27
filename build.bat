@echo off
rem Builds Fidget.exe from main.cpp + every fidgets\*.cpp. Run tests with: build.bat test
cd /d "%~dp0"
if "%1"=="test" (
    for %%t in (typing cube) do (
        g++ -std=c++17 tests\%%t_test.cpp -o %%t_test.exe -lgdiplus -lgdi32 && .\%%t_test.exe || exit /b 1
    )
    exit /b
)
set SRC=main.cpp
for %%f in (fidgets\*.cpp) do call set SRC=%%SRC%% %%f
g++ -O2 -std=c++17 -municode -mwindows -static %SRC% -o Fidget.exe -lgdiplus -lgdi32 && echo Built Fidget.exe
