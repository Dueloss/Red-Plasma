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
#include "Engine.h"
#include <iostream>

#include "VulkanGraphicsDevice.h"

namespace RedPlasma {

    Engine::Engine() : m_IsRunning(false), m_GraphicsDevice(nullptr){
        std::cout << "Red Plasma Engine: Initializing..." << std::endl;
        m_GraphicsDevice = new VulkanGraphicsDevice();
    }

    Engine::~Engine() {
        std::cout << "Red Plasma Engine: Shutting down..." << std::endl;
        if (m_GraphicsDevice) {
            m_GraphicsDevice->Shutdown();
            delete m_GraphicsDevice;
            m_GraphicsDevice = nullptr;
        }
    }

    int Engine::AttachWindow(IWindowSurface* windowHandle) {
        if (m_GraphicsDevice == nullptr || windowHandle == nullptr) {
            std::cout << "Red Plasma Engine: Failed to attach window!" << std::endl;
            return -1;
        }

        m_GraphicsDevice->AddExtension(windowHandle->GetRequiredExtensions());

        if (m_GraphicsDevice->Initialize() == 0) {
            if (m_GraphicsDevice->CreateSurface(windowHandle) == 0) {
                m_IsRunning = true;
                return 0;
            }

        }
        return -1;
    }

    void Engine::Run() const {
        if (m_IsRunning) {
            std::cout << "Red Plasma Engine: Running..." << std::endl;
        }

    }
}
