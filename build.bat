@echo off
echo Compiling...

g++ main.cpp glad.c -I./include -L./lib -o gravity.exe -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build Failed!
) else (
    echo [SUCCESS] Build Complete!
    echo.
    gravity.exe
)