#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>

using namespace std;

const float TASKBAR_HEIGHT = 56.0f;

/* =================================== */
/* OS states / Startup initializations */
/* =================================== */
bool notesOpen = false;
bool settingsOpen = false;
bool taskManagerOpen = false;
bool shutdownPopup = false;
bool poweredOff = false;

// Default wallpaper settings (can be changed from the Settings app)
ImVec4 wallpaperTop = ImVec4(0.10f, 0.20f, 0.45f, 1.0f);
ImVec4 wallpaperBottom = ImVec4(0.45f, 0.15f, 0.40f, 1.0f);
bool showPattern = true;

// Initialize empty Notes app
char notesText[2048] = "";

/* ================= */
/* Desktop wallpaper */
/* ================= */
void drawWallpaper(ImVec2 screen){
    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    ImU32 top = ImGui::ColorConvertFloat4ToU32(wallpaperTop);
    ImU32 bottom = ImGui::ColorConvertFloat4ToU32(wallpaperBottom);

    // Vertical gradient
    bg->AddRectFilledMultiColor(ImVec2(0, 0), screen, top, top, bottom, bottom);

    // Grid pattern
    if (showPattern){
        for (float x = 20; x < screen.x; x += 40){
            for (float y = 20; y < screen.y - TASKBAR_HEIGHT; y += 40){
                bg->AddCircleFilled(ImVec2(x, y), 1.5f, IM_COL32(255, 255, 255, 40));
            }
        }
    }

    // OS name in the middle of the desktop
    const char* title = "hehe monke"; // paltan nyo nalng HAHAH
    float scale = 5.0f;
    ImVec2 size = ImGui::CalcTextSize(title);
    ImVec2 pos = ImVec2((screen.x - size.x * scale) / 2, (screen.y - TASKBAR_HEIGHT - size.y * scale) / 2);
    bg->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, pos, IM_COL32(255, 255, 255, 70), title);
}

/* ==================== */
/* Taskbar icon buttons */
/* ==================== */
enum IconType { ICON_NOTES, ICON_SETTINGS, ICON_TASKMGR };

bool iconButton(const char* id, IconType icon, bool isOpen, const char* tooltip){
    ImVec2 size = ImVec2(42, 42);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, size);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Button background (lighter when hovered)
    ImU32 bgColor = ImGui::IsItemHovered() ? IM_COL32(255, 255, 255, 50) : IM_COL32(255, 255, 255, 15);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bgColor, 6.0f);

    float cx = p.x + size.x / 2;
    float cy = p.y + size.y / 2;
    ImU32 white = IM_COL32(235, 235, 235, 255);

    if (icon == ICON_NOTES){ // A sheet of paper with lines
        dl->AddRectFilled(ImVec2(cx - 9, cy - 12), ImVec2(cx + 9, cy + 12), IM_COL32(250, 220, 90, 255), 2.0f);
        for (int i = 0; i < 4; i++){
            float y = cy - 6 + i * 5;
            dl->AddLine(ImVec2(cx - 5, y), ImVec2(cx + 5, y), IM_COL32(120, 90, 20, 255), 1.5f);
        }
    } else if (icon == ICON_SETTINGS){ // A gear / cogwheel
        dl->AddCircleFilled(ImVec2(cx, cy), 9, white, 8);
        for (int i = 0; i < 8; i++){
            float a = i * 3.14159f / 4;
            ImVec2 tooth = ImVec2(cx + cosf(a) * 11, cy + sinf(a) * 11);
            dl->AddCircleFilled(tooth, 3, white);
        }
        dl->AddCircleFilled(ImVec2(cx, cy), 4, IM_COL32(40, 40, 50, 255));
    } else if (icon == ICON_TASKMGR){ // A small bar chart
        dl->AddRectFilled(ImVec2(cx - 11, cy + 2),  ImVec2(cx - 5, cy + 11), IM_COL32(90, 200, 120, 255));
        dl->AddRectFilled(ImVec2(cx - 3,  cy - 6),  ImVec2(cx + 3, cy + 11), IM_COL32(90, 160, 240, 255));
        dl->AddRectFilled(ImVec2(cx + 5,  cy - 11), ImVec2(cx + 11, cy + 11), IM_COL32(240, 120, 90, 255));
    }

    if (isOpen){ // a small bar under the icon when that app is open (like Windows 11)
        dl->AddRectFilled(ImVec2(cx - 8, p.y + size.y - 3), ImVec2(cx + 8, p.y + size.y), IM_COL32(120, 190, 255, 255), 2.0f);
    }

    if (ImGui::IsItemHovered()){
        ImGui::SetTooltip("%s", tooltip);
    }

    return clicked;
}

/* ==================================================== */
/* Taskbar (PWR button, app icons, running apps, clock) */
/* ==================================================== */
void drawTaskbar(ImVec2 screen){
    ImGui::SetNextWindowPos(ImVec2(0, screen.y - TASKBAR_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(screen.x, TASKBAR_HEIGHT));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Taskbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    // PWR button
    ImGui::SetCursorPosY(7);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
    if (ImGui::Button("PWR", ImVec2(52, 42))){
        shutdownPopup = true;
    }
    ImGui::PopStyleColor(2);

    // App icons
    ImGui::SameLine(0, 20);
    if (iconButton("##notes", ICON_NOTES, notesOpen, "Notes")) notesOpen = !notesOpen;
    ImGui::SameLine();
    if (iconButton("##settings", ICON_SETTINGS, settingsOpen, "Settings")) settingsOpen = !settingsOpen;
    ImGui::SameLine();
    if (iconButton("##taskmgr", ICON_TASKMGR, taskManagerOpen, "Task Manager")) taskManagerOpen = !taskManagerOpen;

    // Running apps
    ImGui::SameLine(0, 30);
    ImGui::SetCursorPosY(18);
    ImGui::TextDisabled("Running:");
    if (notesOpen) { 
		ImGui::SameLine(); if (ImGui::SmallButton("Notes")) ImGui::SetWindowFocus("Notes"); 
	}
    if (settingsOpen) { 
		ImGui::SameLine(); if (ImGui::SmallButton("Settings")) ImGui::SetWindowFocus("Settings"); 
	}
    if (taskManagerOpen) { 
		ImGui::SameLine(); if (ImGui::SmallButton("Task Manager")) ImGui::SetWindowFocus("Task Manager"); 
	}
    if (!notesOpen && !settingsOpen && !taskManagerOpen) {
        ImGui::SameLine(); ImGui::TextDisabled("(none)");
    }

    // Real-time clock and date
    time_t now = time(nullptr);
    tm* local = localtime(&now);
    char timeText[32], dateText[32];
    strftime(timeText, sizeof(timeText), "%I:%M:%S %p", local);
    strftime(dateText, sizeof(dateText), "%a, %b %d %Y", local);

    float clockWidth = ImGui::CalcTextSize(dateText).x;
    ImGui::SetCursorPos(ImVec2(screen.x - clockWidth - 20, 9));
    ImGui::Text("%s", timeText);
    ImGui::SetCursorPos(ImVec2(screen.x - clockWidth - 20, 29));
    ImGui::Text("%s", dateText);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

/* ============ */
/* App 1: Notes */
/* ============ */
void drawNotes(){
    ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Notes", &notesOpen)){
        ImGui::Text("File   Edit   View");
        ImGui::Separator();
        ImGui::InputTextMultiline("##notes", notesText, sizeof(notesText), ImVec2(-1, -ImGui::GetFrameHeightWithSpacing()));
        if (notesText[0] == '\0'){
            ImGui::TextDisabled("Start typing your notes here...");
        } else {
    		ImGui::TextDisabled("%d characters", (int)strlen(notesText));
    	}
    }
    ImGui::End();
}

/* =============== */
/* App 2: Settings */
/* =============== */
void drawSettings(){
    ImGui::SetNextWindowPos(ImVec2(560, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 340), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Settings", &settingsOpen)){
        ImGui::SeparatorText("Personalization");
        ImGui::ColorEdit3("Wallpaper top", (float*)&wallpaperTop);
        ImGui::ColorEdit3("Wallpaper bottom", (float*)&wallpaperBottom);
        ImGui::Checkbox("Show dot pattern", &showPattern);

        ImGui::SeparatorText("About this PC");
        ImGui::Text("OS name    : CSOPESY G5");
        ImGui::Text("Version    : 6.7");
        ImGui::Text("Processor  : AMD Ryzen Threadripper PRO 7995WX");
        ImGui::Text("Memory     : 4.0 GB");
        ImGui::Text("Frame rate : %.0f FPS", ImGui::GetIO().Framerate);
    }
    ImGui::End();
}

/* =================== */
/* App 3: Task Manager */
/* =================== */
struct Process {
    const char* name;
    int pid;
    float cpu; // percent
    float memory; // MB
    const char* status;
};

void drawTaskManager(){
    ImGui::SetNextWindowPos(ImVec2(260, 440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 380), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Task Manager", &taskManagerOpen)){ // dummy process list
        Process processes[] = {
            {"System",              4, 0.4f,   0.1f, "Running"},
            {"Desktop Compositor", 812, 2.1f,  88.4f, "Running"},
            {"Taskbar",            904, 0.3f,  24.7f, "Running"},
            {"Notes",             1320, 0.0f,  12.3f, notesOpen ? "Running" : "Suspended"},
            {"Settings",          1544, 0.1f,  18.9f, settingsOpen ? "Running" : "Suspended"},
            {"Task Manager",      1688, 1.2f,  30.2f, "Running"},
            {"Antivirus Service", 2032, 3.5f, 142.6f, "Running"},
            {"Audio Service",     2210, 0.2f,   9.8f, "Running"},
            {"Network Service",   2398, 0.6f,  15.1f, "Running"},
        };
        int count = sizeof(processes) / sizeof(processes[0]);

        if (ImGui::BeginTabBar("tabs")){
            if (ImGui::BeginTabItem("Processes")){ // totals at the top, like Windows
                float totalCpu = 0, totalMem = 0;
                for (int i = 0; i < count; i++){
                    totalCpu += processes[i].cpu;
                    totalMem += processes[i].memory;
                }
                ImGui::Text("CPU: %.1f%%     Memory: %.1f MB     Processes: %d", totalCpu, totalMem, count);

                ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
                if (ImGui::BeginTable("processes", 5, flags)){
                    ImGui::TableSetupScrollFreeze(0, 1); // keep header visible
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("PID");
                    ImGui::TableSetupColumn("Status");
                    ImGui::TableSetupColumn("CPU");
                    ImGui::TableSetupColumn("Memory");
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < count; i++){
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%s", processes[i].name);
                        ImGui::TableNextColumn(); ImGui::Text("%d", processes[i].pid);
                        ImGui::TableNextColumn(); ImGui::Text("%s", processes[i].status);
                        ImGui::TableNextColumn(); ImGui::Text("%.1f%%", processes[i].cpu);
                        ImGui::TableNextColumn(); ImGui::Text("%.1f MB", processes[i].memory);
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Performance")){
                ImGui::Text("CPU");
                ImGui::ProgressBar(0.67f, ImVec2(-1, 0), "67%");
                ImGui::Text("Memory");
                ImGui::ProgressBar(0.975f, ImVec2(-1, 0), "3.9 / 4.0 GB (97.5%)");
                ImGui::Text("Disk");
                ImGui::ProgressBar(0.99f, ImVec2(-1, 0), "99%");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

/* ===================== */
/* Shutdown confirmation */
/* ===================== */
void drawShutdownPopup(){
    if (shutdownPopup){
        ImGui::OpenPopup("Shut down");
        shutdownPopup = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Shut down", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Are you sure you want to shut down CSOPESY G5?");
        ImGui::Spacing();
        if (ImGui::Button("Shut down", ImVec2(120, 0))){
            poweredOff = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/* ====================================================== */
/* Main: setup, then compositor loop (one frame per pass) */
/* ====================================================== */
int main(){
    if (!glfwInit()){
        return 1;
    }

    // OpenGL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Borderless window (no window control buttons / min-max-close buttons)
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "CSOPESY G5", nullptr, nullptr);
    if (window == nullptr){
        return 1;
    }
    glfwSetWindowPos(window, 0, 0);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Dear ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // don't save window positions to a file
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 8.0f;
    ImGui::GetStyle().FrameRounding  = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // The OS only stops when the PWR button is used
    while (!poweredOff){
        glfwPollEvents();

        // Block force-exits (Alt+F4)
        if (glfwWindowShouldClose(window)){
            glfwSetWindowShouldClose(window, GLFW_FALSE);
        }

        // Start a new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImVec2 screen = ImGui::GetIO().DisplaySize;

        // Draw order: desktop first, then taskbar and app windows on top
        drawWallpaper(screen);
        drawTaskbar(screen);
        if (notesOpen) drawNotes();
        if (settingsOpen) drawSettings();
        if (taskManagerOpen) drawTaskManager();
        drawShutdownPopup();

        // Render the frame
        ImGui::Render();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
