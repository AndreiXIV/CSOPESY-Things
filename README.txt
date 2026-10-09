HOW TO BUILD AND RUN
- Open cmd in this folder/directory
- Copy and paste this command to the cmd, then enter:
g++ -std=c++17 main.cpp imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\imgui_impl_glfw.cpp imgui\imgui_impl_opengl3.cpp -Iimgui -Iglfw\include -Lglfw\lib -lglfw3 -lopengl32 -lgdi32 -static -o CSOPESY-G5-Desktop.exe && CSOPESY-G5-Desktop.exe

FEATURES
- Taskbar        : PWR button, 3 app icons, running apps, date & time
- Notes icon     : opens a mock Notes app ('File', 'Edit', and 'View' features don't work yet)
- Gear icon      : opens a mock Settings (change wallpaper + PC info/specs)
- Bar-chart icon : opens a mock Task Manager (placeholder 'Processes' & 'Performance')
- PWR            : the only way to close the OS (with confirmation modal)
                 : Alt+F4 is ignored