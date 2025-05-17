# SaHack-CS2
C++ Cheats for CS2 by Sams0n0v


Проект ESP (пример с imgui + glfw + glew + d3dx9)
--------------------------------------------------

Библиотеки:
- Dear ImGui (https://github.com/ocornut/imgui)
- GLFW (https://www.glfw.org/)
- GLEW (http://glew.sourceforge.net/)
- DirectX SDK June 2010 (для d3dx9.lib)

Структура проекта:
- main.cpp - пример исходного кода
- imgui/ - исходники Dear ImGui и backends
- include/ - заголовочные файлы GLFW, GLEW, d3dx9
- libs/ - бинарные файлы библиотек (lib и dll)

Пример сборки с g++ (MinGW):
g++ main.cpp imgui/*.cpp imgui/backends/*.cpp -Iinclude -Llibs -lglew32 -lglfw3 -ld3dx9 -lopengl32 -lgdi32 -o esp.exe

В Visual Studio нужно подключить все cpp из imgui и backends,
указать пути к include и libs, добавить ссылки на glew32.lib, glfw3.lib, d3dx9.lib.
