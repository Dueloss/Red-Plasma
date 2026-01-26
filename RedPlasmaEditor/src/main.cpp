// /*
//  * Red Plasma Engine
//  * Copyright (C) 2026  Kim Johansson
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License as published by
//  * the Free Software Foundation...
//  *

//
// Created by Dueloss on 12.01.2026.
//

#include <iostream>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#include "core/Engine.h"
#include "plugins/renderer/vulkan/platform/linux/wayland/WaylandSurface.h"


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    int width = 1280, height = 720;
    GLFWwindow* window = glfwCreateWindow(width, height, "Red Plasma Editor", nullptr, nullptr);

    std::cout << "[Editor] Red Plasma Engine: Starting..." << std::endl;
    RedPlasma::Engine engine;

    void* wl_display = glfwGetWaylandDisplay();
    void* wl_surface = glfwGetWaylandWindow(window);

    auto* mySurface = new RedPlasma::WaylandSurface(wl_display, wl_surface, width, height);
    mySurface->UpdateSize(width, height);

    engine.AttachWindow(mySurface);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        engine.Run();
    }
    engine.Shutdown();
    std::cout << "[Editor] Red Plasma Engine: Closing..." << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
