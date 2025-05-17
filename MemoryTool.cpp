// INTERNAL = BAN. 
// INTERNAL = BAN. 
// INTERNAL = BAN. 

#include <array>
#include <limits>
#include <algorithm>
#include <string>
#include <cmath>
#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <d3dx9math.h>
#pragma comment(lib, "d3dx9.lib")


#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// --- Vector ---
struct Vector {
    float x, y, z;
    Vector() = default;
    Vector(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vector operator-(float val) const {
        return Vector{x - val, y - val, z - val};
    }

    Vector operator+(float val) const {
        return Vector{x + val, y + val, z + val};
    }
};

// --- Заглушка worldToScreen ---
// Преобразует 3D точку в 2D экранные координаты.
// Возвращает true, если точка видна на экране.
// Нужно считать viewMatrix из памяти игры или задать вручную (если известен адрес).
bool worldToScreen(const Vector& worldPos, ImVec2& screenPos) {
    D3DXVECTOR4 clipCoords;
    clipCoords.x = worldPos.x * viewMatrix._11 + worldPos.y * viewMatrix._21 + worldPos.z * viewMatrix._31 + viewMatrix._41;
    clipCoords.y = worldPos.x * viewMatrix._12 + worldPos.y * viewMatrix._22 + worldPos.z * viewMatrix._32 + viewMatrix._42;
    clipCoords.z = worldPos.x * viewMatrix._13 + worldPos.y * viewMatrix._23 + worldPos.z * viewMatrix._33 + viewMatrix._43;
    clipCoords.w = worldPos.x * viewMatrix._14 + worldPos.y * viewMatrix._24 + worldPos.z * viewMatrix._34 + viewMatrix._44;

    if (clipCoords.w < 0.1f)
        return false;

    D3DXVECTOR3 NDC;
    NDC.x = clipCoords.x / clipCoords.w;
    NDC.y = clipCoords.y / clipCoords.w;

    // Преобразование в координаты экрана
    screenPos.x = (1280 / 2 * NDC.x) + (NDC.x + 1280 / 2);
    screenPos.y = -(720 / 2 * NDC.y) + (NDC.y + 720 / 2);

  // uintptr_t FindPattern(HANDLE hProcess, uintptr_t base, DWORD size, const char* pattern, const char* mask); >>> Маска для памяти. Нужно использовать CheatEngine

    return true;
}

// --- Данные игрока ---
struct PlayerData {
    Vector obbMins;
    Vector obbMaxs;
    int health;
    float distanceToLocal;
    std::string name;
    bool alive;
    bool enemy;

    PlayerData(Vector mins, Vector maxs, int hp, float dist, std::string n, bool a, bool e)
        : obbMins(mins), obbMaxs(maxs), health(hp), distanceToLocal(dist), name(std::move(n)), alive(a), enemy(e) {}
};

// --- Конфиг для ESP ---
struct ESPConfig {
    bool enable = true;
    bool box = true;
    bool healthBar = true;
    bool health = true;
    bool name = true;
};

ESPConfig* config = new ESPConfig();

// --- Меню и шрифты ---
struct Menu {
    struct Fonts {
        ImFont* roboto = nullptr;
    } fonts;
};

Menu* menu = new Menu();

// --- BoundingBox ---
struct BoundingBox {
private:
    bool valid = false;
public:
    ImVec2 min, max;
    std::array<ImVec2, 8> vertices;

    BoundingBox(const Vector& mins, const Vector& maxs) noexcept
    {
        min.y = min.x = std::numeric_limits<float>::max();
        max.y = max.x = -std::numeric_limits<float>::max();

        for (int i = 0; i < 8; ++i) {
            Vector point{ i & 1 ? maxs.x : mins.x, i & 2 ? maxs.y : mins.y, i & 4 ? maxs.z : mins.z };
            if (!worldToScreen(point, vertices[i])) {
                valid = false;
                return;
            }

            min.x = std::min(min.x, vertices[i].x);
            min.y = std::min(min.y, vertices[i].y);
            max.x = std::max(max.x, vertices[i].x);
            max.y = std::max(max.y, vertices[i].y);
        }
        valid = true;
    }

    BoundingBox(const PlayerData& data) noexcept : BoundingBox{ data.obbMins, data.obbMaxs } {}

    operator bool() const noexcept
    {
        return valid;
    }
};

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 modEntry;
        modEntry.dwSize = sizeof(modEntry);

        if (Module32First(hSnap, &modEntry)) {
            do {
                if (!_wcsicmp(modEntry.szModule, modName)) {
                    modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32Next(hSnap, &modEntry));
        }
    }
    CloseHandle(hSnap);
    return modBaseAddr;
}

DWORD GetProcId(const wchar_t* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);
        if (Process32First(hSnap, &procEntry)) {
            do {
                if (!_wcsicmp(procEntry.szExeFile, procName)) {
                    procId = procEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procId;
}

template<typename T>
T ReadMemory(HANDLE hProcess, uintptr_t address) {
    T buffer;
    ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), nullptr);
    return buffer;
}

void RunMemoryReader(const wchar_t* procName, const wchar_t* moduleName, uintptr_t playerBaseOffset, uintptr_t xOffset, uintptr_t yOffset, uintptr_t zOffset) {
    DWORD procId = GetProcId(procName);

    if (!procId) {
        std::wcerr << L"Процесс не найден: " << procName << std::endl;
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, procId);
    if (!hProcess) {
        std::cerr << "Не удалось открыть процесс." << std::endl;
        return;
    }

    uintptr_t modBase = GetModuleBaseAddress(procId, moduleName);
    if (!modBase) {
        std::cerr << "Не удалось получить base address модуля." << std::endl;
        CloseHandle(hProcess);
        return;
    }

    uintptr_t playerBaseAddr = modBase + playerBaseOffset;

    while (true) {
        float x = ReadMemory<float>(hProcess, playerBaseAddr + xOffset);
        float y = ReadMemory<float>(hProcess, playerBaseAddr + yOffset);
        float z = ReadMemory<float>(hProcess, playerBaseAddr + zOffset);

        std::cout << "X: " << x << " Y: " << y << " Z: " << z << std::endl;

        Sleep(1000); // Пауза в 1 сек
    }

    CloseHandle(hProcess);
}

static ImDrawList* drawList = nullptr;
static void drawHealthBar(ImVec2 pos, float height, int health) noexcept
{
    constexpr float width = 4.f;

    ImVec2 min = pos;
    ImVec2 max = min + ImVec2{ width, height };

    drawList->PushClipRect(pos + ImVec2{ 0.0f, (100 - health) / 100.0f * height }, pos + ImVec2{ width + 1.0f, height + 1.0f });
    drawList->AddRectFilled(ImFloor(min), ImFloor(max), ImColor(1.0f, 0.0f, 0.0f, 1.0f));
    drawList->PopClipRect();
}

// --- враг ---
static void renderEnemy(const PlayerData& playerData) noexcept
{
    const BoundingBox bbox{ playerData };
    if (!bbox)
        return;

    if (config->box)
    {
        drawList->AddRect(bbox.min, bbox.max, ImColor(0, 0, 0, 255));
    }

    if (config->healthBar)
    {
        drawHealthBar(bbox.min - ImVec2(5.0f, 0.0f), (bbox.max.y - bbox.min.y), playerData.health);
    }

    float text_size = std::clamp(15 * 150 / playerData.distanceToLocal, 10.0f, 15.0f);
    float size_modifier = text_size / 15.0f;
    auto space = 15 * size_modifier;

    auto x_pos = bbox.max.x + 5 * size_modifier;
    auto y_pos = bbox.min.y - 3;

    if (config->health)
    {
        std::string health_str = std::to_string(playerData.health);
        drawList->AddText(ImGui::GetFont(), text_size, ImVec2(x_pos, y_pos), ImColor(0, 0, 0, 255), health_str.c_str());
    }

    if (config->name)
    {
        std::string& name = playerData.name;
        ImVec2 text_size_vec = ImGui::CalcTextSize(name.c_str(), nullptr, false, FLT_MAX);
        drawList->AddText(ImGui::GetFont(), text_size, ImVec2((bbox.min.x + bbox.max.x) / 2 - text_size_vec.x / 2, bbox.min.y - space), ImColor(0, 0, 0, 255), name.c_str());
    }
}

// --- ESP Render ---
void ESPRender(ImDrawList* list, const std::vector<PlayerData>& players, bool localPlayerExists)
{
    if (!config->enable)
        return;

    if (!localPlayerExists)
        return;

    drawList = list;

    for (const auto& player : players)
    {
        if (player.alive && player.enemy)
            renderEnemy(player);
    }
}

std::vector<PlayerData> GetPlayers(HANDLE processHandle, uintptr_t baseAddress, int localPlayerId) {
    std::vector<PlayerData> players;
    const size_t playerSize = 0x1F0;

    for (int i = 0; i < 32; ++i) {
        uintptr_t playerAddr = baseAddress + (i * playerSize);

        Vector position, head;
        float angle = 0.0f;
        int health = 0;
        char nameBuffer[32] = {};

        ReadProcessMemory(processHandle, (LPCVOID)(playerAddr + 0x34), &position, sizeof(Vector), nullptr);
        ReadProcessMemory(processHandle, (LPCVOID)(playerAddr + 0x40), &head, sizeof(Vector), nullptr);
        ReadProcessMemory(processHandle, (LPCVOID)(playerAddr + 0x44), &angle, sizeof(float), nullptr);
        ReadProcessMemory(processHandle, (LPCVOID)(playerAddr + 0xF8), &health, sizeof(int), nullptr);
        ReadProcessMemory(processHandle, (LPCVOID)(playerAddr + 0x225), &nameBuffer, sizeof(nameBuffer), nullptr);

        std::string name = nameBuffer;

        if (health > 0 && health <= 100 && !name.empty()) {
            float distance = sqrt(position.x * position.x + position.y * position.y + position.z * position.z); // или к localPlayer
            players.push_back(PlayerData(head, position, angle, distance, name, true, i != localPlayerId));
        }
    }

    return players;
}


int main()
{
    if (!glfwInit())
        return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "ESP", NULL, NULL);
    if (window == NULL)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }

    glfwSwapInterval(1); // VSync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->AddFontDefault();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    players = GetPlayers(processHandle, playersBase, localPlayerId); // Нужно Получить processHandle с правами PROCESS_VM_READ и найти playersBase в Cheat Engine или вручную.

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

        ESPRender(draw_list, players, true);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    const wchar_t* procName = L"cs2.exe";               // имя процесса игры
    const wchar_t* moduleName = L"cs2.exe";             // имя основного модуля
    // ДЛЯ ТОГО, ЧТОБЫ ЧИТ РАБОТАЛ, НУЖНО ЗАМЕНИТЬ ОАДРЕСА НИЖЕ НА РЕАЛЬНЫЕ. 
    // ТУТ ИСПОЛЬЗУЮТСЯ СТАРЫЕ ОФФСЕТЫ (CS 1.6, CSS)
    // НУЖНО НАЙТИ АДРЕСА ВРУЧНУЮ ЧЕРЕЗ Cheat Engine
    uintptr_t playerBaseOffset = 0x0010F4F4;             // оффсет до базы игрока
    uintptr_t xOffset = 0x30;                            // смещение до координаты X
    uintptr_t yOffset = 0x34;                            // смещение до координаты Y
    uintptr_t zOffset = 0x38;                            // смещение до координаты Z

    RunMemoryReader(procName, moduleName, playerBaseOffset, xOffset, yOffset, zOffset);

    ImDrawList* drawListExample = ImGui::GetForegroundDrawList();

    return 0;
}
