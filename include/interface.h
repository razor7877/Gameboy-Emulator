#ifndef INTERFACE_H
#define INTERFACE_H

#include <GLFW/glfw3.h>

static void glfw_error_callback(int error, const char* description);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

int setup_glfw();
int setup_ImGui();

void render_ImGui();
void update_texture();

int start_interface();
int update_interface();
void stop_interface();

#endif