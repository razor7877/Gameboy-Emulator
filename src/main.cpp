#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <iostream>

#include <GLFW/glfw3.h>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/imgui_memory_editor.h"
#include <stdio.h>

GLFWwindow* window;
const char* glsl_version;

// 160px * 144px * 4 bytes per pixel (RGBA)
uint8_t display[160 * 144 * 4]{};
GLuint display_texture{};

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

// Registers
// They are grouped two by two. AF corresponds to registers A (upper 8 bits) and F (lower 8 bits)
// F is the flag register
uint16_t AF{};
uint16_t BC{};
uint16_t DE{};
uint16_t HL{};

uint16_t sp{}; // Stack pointer
uint16_t pc = 0x100; // Program counter

uint8_t memory[0x10000]{};

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int setup_glfw()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return -1;

    // GL 3.0 + GLSL 130
    glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    window = glfwCreateWindow(1920, 1080, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    return 0;
}

int setup_ImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return 0;
}

void update_texture()
{
    glGenTextures(1, &display_texture);
    glBindTexture(GL_TEXTURE_2D, display_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 160, 144, 0, GL_RGBA, GL_UNSIGNED_BYTE, display);
}

void render_ImGui()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Gameboy display
    {
        ImGui::Begin("Display");
        ImGui::Image((void*)(intptr_t)display_texture, ImVec2(160, 144));
        ImGui::End();
    }

    // CPU
    {
        ImGui::Begin("CPU", 0, ImGuiWindowFlags_NoResize);
        ImGui::SetWindowSize(ImVec2(200, 280));
        ImGui::Text("Registers (hex/decimal):");
        ImGui::Text("AF: 0x%x / %d", AF, AF);
        ImGui::Text("BC: 0x%x / %d", BC, BC);
        ImGui::Text("DE: 0x%x / %d", HL, DE);
        ImGui::Text("HL: 0x%x / %d", HL, HL);

        ImGui::NewLine();

        ImGui::Text("pc: 0x%x / %d", pc, pc);
        ImGui::Text("sp: 0x%x / %d", sp, sp);

        ImGui::NewLine();

        ImGui::Text("Flags (F register):");

        ImGui::Text("Zero: %d", (AF & 0b10000000) >> 7); // Get 7th bit
        ImGui::Text("Substraction (BCD): %d", (AF & 0b01000000) >> 6); // Get 6th bit
        ImGui::Text("Half Carry (BCD): %d", (AF & 0b00100000) >> 5); // Get 5th bit
        ImGui::Text("Carry: %d", (AF & 0b00010000) >> 4); // Get 4th bit

        ImGui::End();
    }

    // Memory explorer
    {
        static MemoryEditor mem_edit;
        mem_edit.DrawWindow("Memory Editor", memory, sizeof(memory));
    }

    {
        ImGui::Begin("Debug");

        if (ImGui::Button("Trigger breakpoint"))
        {
            std::cout << "Break point" << std::endl;
        }
        
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

// Main code
int main(int, char**)
{
    if (setup_glfw() != 0)
        return -1;

    setup_ImGui();

    for (int x = 0; x < (160 * 144 * 4); x += 4)
    {
        display[x] = 255;
        display[x + 1] = 0;
        display[x + 2] = 0;
        display[x + 3] = 255;
    }

    update_texture();

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        render_ImGui();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
