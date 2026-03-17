@echo off
echo Compiling...

if not exist imgui.o (
    echo Building ImGui library - this will take a minute, but only happens once...
    g++ -c include/imgui/imgui.cpp -isystem ./include -isystem ./include/imgui -o imgui.o
    g++ -c include/imgui/imgui_draw.cpp -isystem ./include -isystem ./include/imgui -o imgui_draw.o
    g++ -c include/imgui/imgui_tables.cpp -isystem ./include -isystem ./include/imgui -o imgui_tables.o
    g++ -c include/imgui/imgui_widgets.cpp -isystem ./include -isystem ./include/imgui -o imgui_widgets.o
    g++ -c include/imgui/imgui_impl_glfw.cpp -isystem ./include -isystem ./include/imgui -o imgui_impl_glfw.o
    g++ -c include/imgui/imgui_impl_opengl3.cpp -isystem ./include -isystem ./include/imgui -DIMGUI_IMPL_OPENGL_LOADER_GLAD -o imgui_impl_opengl3.o
    g++ -c glad.c -isystem ./include -o glad.o
)

echo Building GravitySim3D...
g++ main.cpp imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_glfw.o imgui_impl_opengl3.o glad.o -isystem ./include -isystem ./include/imgui -L./lib -DIMGUI_IMPL_OPENGL_LOADER_GLAD -o gravity.exe -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build Failed!
) else (
    echo [SUCCESS] Build Complete!
    echo.
    gravity.exe
)